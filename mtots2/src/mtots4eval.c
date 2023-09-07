#include "mtots4eval.h"

#include <stdio.h>

#include "mtots1err.h"
#include "mtots2map.h"
#include "mtots4function.h"
#include "mtots4globals.h"

#define FRAME_STACK_SIZE 1024
#define EVAL_STACK_SIZE 1024

typedef struct Frame {
  u16 evalStart;
} Frame;

static Value evalStack[EVAL_STACK_SIZE];
static Frame frameStack[FRAME_STACK_SIZE];
static u16 evalStackSize;
static u16 frameStackSize;
static Map *globals;

void freeGlobals(void) {
  if (globals) {
    releaseMap(globals);
    globals = NULL;
  }
}

static Map *getGlobals(void) {
  if (!globals) {
    globals = newGlobals();
  }
  return globals;
}

/** NOTE: Retain/release not done automatically */
static void evalStackPush(Value value, ubool retain) {
  if (evalStackSize >= EVAL_STACK_SIZE) {
    panic("stackoverflow (evalStackPush)");
  }
  if (retain) {
    retainValue(value);
  }
  evalStack[evalStackSize++] = value;
}

/** NOTE: Retain/release not done automatically */
static Value evalStackPop(ubool release) {
  if (evalStackSize == 0) {
    panic("stackunderflow (evalStackPop)");
  }
  if (release) {
    releaseValue(evalStack[evalStackSize - 1]);
  }
  return evalStack[--evalStackSize];
}

static void pushFrame(void) {
  Frame *frame = &frameStack[frameStackSize++];
  frame->evalStart = evalStackSize;
}

static void popFrame(void) {
  Frame *frame;
  if (frameStackSize == 0) {
    panic("stackunderflow (popFrame)");
  }
  frame = &frameStack[--frameStackSize];
  while (evalStackSize > frame->evalStart) {
    releaseValue(evalStack[--evalStackSize]);
  }
}

Status evalAst(Ast *node) {
  switch (node->type) {
    case AST_MODULE: {
      AstModule *block = (AstModule *)node;
      Ast *child;
      pushFrame();
      for (child = block->first; child; child = child->next) {
        if (!evalAst(child)) {
          return STATUS_ERR;
        }
        evalStackPop(UTRUE);
      }
      popFrame();
      /* TODO: Return a Module object */
      evalStackPush(nilValue(), UFALSE);
      return STATUS_OK;
    }
    case AST_LITERAL:
      evalStackPush(((AstLiteral *)node)->value, UTRUE);
      return STATUS_OK;
    case AST_GET_VAR: {
      Symbol *symbol = ((AstGetVar *)node)->symbol;
      Value value = mapGet(getGlobals(), symbolValue(symbol));
      if (isSentinel(value)) {
        printValue(mapValue(getGlobals()));
        runtimeError("Variable '%s' not found",
                     symbolChars(symbol));
        return STATUS_ERR;
      }
      evalStackPush(value, UTRUE);
      return STATUS_OK;
    }
    case AST_SET_VAR: {
      AstSetVar *setVar = (AstSetVar *)node;
      Symbol *symbol = setVar->symbol;
      if (!evalAst(setVar->value)) {
        return STATUS_ERR;
      }
      mapSet(getGlobals(), symbolValue(symbol), evalStack[evalStackSize - 1]);
      return STATUS_OK;
    }
    case AST_BLOCK: {
      /* TODO: scoping */
      AstBlock *block = (AstBlock *)node;
      Ast *child;
      for (child = block->first; child; child = child->next) {
        if (!evalAst(child)) {
          return STATUS_ERR;
        }
        evalStackPop(UTRUE);
      }
      evalStackPush(nilValue(), UFALSE);
      return STATUS_OK;
    }
    case AST_UNOP: {
      AstUnop *op = (AstUnop *)node;
      Value arg, *ret;
      if (!evalAst(op->arg)) {
        return STATUS_ERR;
      }
      arg = evalStack[evalStackSize - 1];
      ret = &evalStack[evalStackSize - 1];
      switch (op->type) {
        case UNOP_POSITIVE:
          break;
        case UNOP_NEGATIVE:
          *ret = negateValue(arg);
          break;
      }
      releaseValue(arg);
      return STATUS_OK;
    }
    case AST_BINOP: {
      AstBinop *op = (AstBinop *)node;
      Value lhs, rhs, *ret;
      if (!evalAst(op->args)) {
        return STATUS_ERR;
      }
      if (!evalAst(op->args->next)) {
        return STATUS_ERR;
      }
      lhs = evalStack[evalStackSize - 2];
      rhs = evalStack[evalStackSize - 1];
      ret = &evalStack[evalStackSize - 2];
      evalStackSize--;
      switch (op->type) {
        case BINOP_ADD:
          *ret = addValues(lhs, rhs);
          break;
        case BINOP_SUBTRACT:
          *ret = subtractValues(lhs, rhs);
          break;
        case BINOP_MULTIPLY:
          *ret = multiplyValues(lhs, rhs);
          break;
        case BINOP_MODULO:
          *ret = moduloValues(lhs, rhs);
          break;
        case BINOP_DIVIDE:
          *ret = divideValues(lhs, rhs);
          break;
        case BINOP_FLOOR_DIVIDE:
          *ret = floorDivideValues(lhs, rhs);
          break;
      }
      releaseValue(rhs);
      releaseValue(lhs);
      return STATUS_OK;
    }
    case AST_LOGICAL: {
      AstLogical *op = (AstLogical *)node;
      switch (op->type) {
        case LOGICAL_NOT: {
          ubool result;
          if (!evalAst(op->args)) {
            return STATUS_ERR;
          }
          result = !testValue(evalStack[evalStackSize - 1]);
          releaseValue(evalStack[evalStackSize - 1]);
          evalStack[evalStackSize - 1] = boolValue(result);
          return STATUS_OK;
        }
        case LOGICAL_OR:
          if (!evalAst(op->args)) {
            return STATUS_ERR;
          }
          if (!testValue(evalStack[evalStackSize - 1])) {
            evalStackPop(UTRUE);
            return evalAst(op->args->next);
          }
          return STATUS_OK;
        case LOGICAL_AND:
          if (!evalAst(op->args)) {
            return STATUS_ERR;
          }
          if (testValue(evalStack[evalStackSize - 1])) {
            evalStackPop(UTRUE);
            return evalAst(op->args->next);
          }
          return STATUS_OK;
        case LOGICAL_IF: {
          ubool condition;
          if (!evalAst(op->args)) {
            return STATUS_ERR;
          }
          condition = testValue(evalStack[evalStackSize - 1]);
          evalStackPop(UTRUE);
          return evalAst(condition ? op->args->next : op->args->next->next);
        }
      }
      panic("INVALID LOGICAL OPERATOR %d", op->type);
    }
    case AST_CALL: {
      AstCall *call = (AstCall *)node;
      Value *argv, func, *out;
      i16 argc, i;
      Ast *argAst;
      out = evalStack + evalStackSize;
      argv = evalStack + evalStackSize + 1;
      argc = -1; /* account for the func/receiver */
      for (argAst = call->funcAndArgs; argAst; argAst = argAst->next) {
        argc++;
        if (!evalAst(argAst)) {
          return STATUS_ERR;
        }
      }
      /* NOTE: We are taking the 'retain' that's on the stack here */
      func = *out;
      if (call->name) {
        if (!callMethod(call->name, argc, argv, out)) {
          return STATUS_ERR;
        }
      } else {
        if (!callValue(func, argc, argv, out)) {
          return STATUS_ERR;
        }
      }
      /* NOTE: callValue should have overwritten `func` on the stack
       * by writing on the stack. The release here is to account for
       * `func` disappearing from the stack */
      releaseValue(func);
      for (i = 0; i < argc; i++) {
        evalStackPop(UTRUE);
      }
      return STATUS_OK;
    }
    case AST_FUNCTION:
      evalStackPush(functionValue(newFunction((AstFunction *)node)), UFALSE);
      return STATUS_OK;
  }
  panic("Unrecognized AST type %d", node->type);
}

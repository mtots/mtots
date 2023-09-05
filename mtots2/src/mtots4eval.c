#include "mtots4eval.h"

#include <stdio.h>

#include "mtots1err.h"
#include "mtots2map.h"
#include "mtots4globals.h"

#define EVAL_STACK_SIZE 1024

static Value evalStack[EVAL_STACK_SIZE];
static u16 evalStackSize;
static Map *globals;

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

Status evalAst(Ast *node) {
  switch (node->type) {
    case AST_LITERAL:
      evalStackPush(((AstLiteral *)node)->value, UTRUE);
      return STATUS_OK;
    case AST_GET_GLOBAL: {
      Symbol *symbol = ((AstGetGlobal *)node)->symbol;
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
    case AST_BLOCK: {
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
    case AST_CALL: {
      AstCall *call = (AstCall *)node;
      Value *argv, func, *out;
      i16 argc = 0, i;
      Ast *argAst;
      out = evalStack + evalStackSize;
      if (!evalAst(call->function)) {
        return STATUS_ERR;
      }
      /* NOTE: We are taking the 'retain' that's on the stack here */
      func = *out;
      argv = evalStack + evalStackSize;
      for (argAst = call->firstArg; argAst; argAst = argAst->next) {
        argc++;
        if (!evalAst(argAst)) {
          return STATUS_ERR;
        }
      }
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
  }
  panic("Unrecognized AST type %d", node->type);
}

#include "mtots3ast.h"

#include <stdio.h>
#include <stdlib.h>

#define NEW_AST(ctype, enumtype) (ctype *)newAst(enumtype, sizeof(ctype))

#if MTOTS_DEBUG_MEMORY_LEAK
static Ast *allAsts;
#endif

static Ast *newAst(AstType type, u32 size) {
  Ast *ast = (Ast *)calloc(1, size);
  ast->type = type;
#if MTOTS_DEBUG_MEMORY_LEAK
  if (allAsts) {
    allAsts->pre = ast;
    ast->suf = allAsts;
  }
  allAsts = ast;
#endif
  return ast;
}

Parameter *newParameter(Symbol *name, Value defaultValue) {
  Parameter *param = (Parameter *)calloc(1, sizeof(Parameter));
  param->name = name;
  param->defaultValue = defaultValue;
  retainValue(defaultValue);
  freezeValue(defaultValue);
  return param;
}

Ast *newAstModule(u32 line, Ast *first) {
  AstModule *ast = NEW_AST(AstModule, AST_MODULE);
  ast->first = first;
  return (Ast *)ast;
}

Ast *newAstLiteral(u32 line, Value value) {
  AstLiteral *ast = NEW_AST(AstLiteral, AST_LITERAL);
  ast->value = value;
  retainValue(value);
  freezeValue(value);
  return (Ast *)ast;
}

Ast *newAstGetVar(u32 line, Symbol *name) {
  AstGetVar *ast = NEW_AST(AstGetVar, AST_GET_VAR);
  ast->symbol = name;
  return (Ast *)ast;
}

Ast *newAstSetVar(u32 line, Symbol *name, Ast *value) {
  AstSetVar *ast = NEW_AST(AstSetVar, AST_SET_VAR);
  ast->symbol = name;
  ast->value = value;
  return (Ast *)ast;
}

Ast *newAstBlock(u32 line, Ast *first) {
  AstBlock *ast = NEW_AST(AstBlock, AST_BLOCK);
  ast->first = first;
  return (Ast *)ast;
}

Ast *newAstUnop(u32 line, UnopType type, Ast *arg) {
  AstUnop *ast = NEW_AST(AstUnop, AST_UNOP);
  ast->type = type;
  ast->arg = arg;
  return (Ast *)ast;
}

Ast *newAstBinop(u32 line, BinopType type, Ast *args) {
  AstBinop *ast = NEW_AST(AstBinop, AST_BINOP);
  ast->type = type;
  ast->args = args;
  return (Ast *)ast;
}

Ast *newAstLogical(u32 line, LogicalType type, Ast *args) {
  AstLogical *ast = NEW_AST(AstLogical, AST_LOGICAL);
  ast->type = type;
  ast->args = args;
  return (Ast *)ast;
}

Ast *newAstCall(u32 line, Symbol *name, Ast *funcAndArgs) {
  AstCall *ast = NEW_AST(AstCall, AST_CALL);
  ast->name = name;
  ast->funcAndArgs = funcAndArgs;
  return (Ast *)ast;
}

Ast *newAstFunction(u32 line, Symbol *name, Parameter *parameters, Ast *body) {
  AstFunction *ast = NEW_AST(AstFunction, AST_FUNCTION);
  ast->name = name;
  ast->parameters = parameters;
  ast->body = body;
  for (; parameters; parameters = parameters->next) {
    if (isSentinel(parameters->defaultValue)) {
      ast->arity++;
    }
    ast->maxArity++;
  }
  return (Ast *)ast;
}

void freeParameter(Parameter *parameter) {
  if (parameter) {
    if (parameter->next) {
      freeParameter(parameter->next);
    }
    releaseValue(parameter->defaultValue);
  }
}

void freeAst(Ast *ast) {
  if (!ast) {
    return;
  }
#if MTOTS_DEBUG_MEMORY_LEAK
  if (ast->pre) {
    ast->pre->suf = ast->suf;
  } else {
    allAsts = ast->suf;
  }
  if (ast->suf) {
    ast->suf->pre = ast->pre;
  }
#endif
  freeAst(ast->next);
  switch (ast->type) {
    case AST_MODULE:
      freeAst(((AstModule *)ast)->first);
      break;
    case AST_LITERAL:
      releaseValue(((AstLiteral *)ast)->value);
      break;
    case AST_GET_VAR:
      break;
    case AST_SET_VAR:
      freeAst(((AstSetVar *)ast)->value);
      break;
    case AST_BLOCK:
      freeAst(((AstBlock *)ast)->first);
      break;
    case AST_UNOP:
      freeAst(((AstUnop *)ast)->arg);
      break;
    case AST_BINOP:
      freeAst(((AstBinop *)ast)->args);
      break;
    case AST_LOGICAL:
      freeAst(((AstLogical *)ast)->args);
      break;
    case AST_CALL:
      freeAst(((AstCall *)ast)->funcAndArgs);
      break;
    case AST_FUNCTION:
      freeParameter(((AstFunction *)ast)->parameters);
      freeAst(((AstFunction *)ast)->body);
      break;
  }
  free(ast);
}

#if MTOTS_DEBUG_MEMORY_LEAK
size_t printLeakedAsts(void) {
  size_t leakCount = 0;
  Ast *node;
  for (node = allAsts; node; node = node->suf) {
    leakCount++;
    printf("[DEBUGDEBUG] LEAKED AST %d at %p\n", node->type, (void *)node);
  }
  return leakCount;
}
#endif

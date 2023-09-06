#include "mtots3ast.h"

#include <stdio.h>
#include <stdlib.h>

#define NEW_AST(ctype, enumtype) (ctype *)newAst(enumtype, sizeof(ctype))

#if MTOTS_DEBUG_MEMORY_LEAK
static Ast *allAsts;
#endif

static Ast *newAst(AstType type, size_t size) {
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

Ast *newAstLiteral(size_t line, Value value) {
  AstLiteral *ast = NEW_AST(AstLiteral, AST_LITERAL);
  ast->value = value;
  retainValue(value);
  return (Ast *)ast;
}

Ast *newAstGetGlobal(size_t line, Symbol *name) {
  AstGetGlobal *ast = NEW_AST(AstGetGlobal, AST_GET_GLOBAL);
  ast->symbol = name;
  return (Ast *)ast;
}

Ast *newAstBlock(size_t line, Ast *first) {
  AstBlock *ast = NEW_AST(AstBlock, AST_BLOCK);
  ast->first = first;
  return (Ast *)ast;
}

Ast *newAstUnop(size_t line, UnopType type, Ast *arg) {
  AstUnop *ast = NEW_AST(AstUnop, AST_UNOP);
  ast->type = type;
  ast->arg = arg;
  return (Ast *)ast;
}

Ast *newAstBinop(size_t line, BinopType type, Ast *args) {
  AstBinop *ast = NEW_AST(AstBinop, AST_BINOP);
  ast->type = type;
  ast->args = args;
  return (Ast *)ast;
}

Ast *newAstLogical(size_t line, LogicalType type, Ast *args) {
  AstLogical *ast = NEW_AST(AstLogical, AST_LOGICAL);
  ast->type = type;
  ast->args = args;
  return (Ast *)ast;
}

Ast *newAstCall(size_t line, Symbol *name, Ast *funcAndArgs) {
  AstCall *ast = NEW_AST(AstCall, AST_CALL);
  ast->name = name;
  ast->funcAndArgs = funcAndArgs;
  return (Ast *)ast;
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
    case AST_LITERAL:
      releaseValue(((AstLiteral *)ast)->value);
      break;
    case AST_GET_GLOBAL:
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
  }
  free(ast);
}

#if MTOTS_DEBUG_MEMORY_LEAK
void printLeakedAsts(void) {
  Ast *node;
  for (node = allAsts; node; node = node->suf) {
    printf("[DEBUGDEBUG] LEAKED AST %d at %p\n", node->type, (void *)node);
  }
}
#endif

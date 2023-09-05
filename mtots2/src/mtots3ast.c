#include "mtots3ast.h"

#include <stdlib.h>

Ast *newAstLiteral(size_t line, Value value) {
  AstLiteral *ast = (AstLiteral *)calloc(1, sizeof(AstLiteral));
  ast->ast.type = AST_LITERAL;
  ast->value = value;
  retainValue(value);
  return (Ast *)ast;
}

Ast *newAstGetGlobal(size_t line, Symbol *name) {
  AstGetGlobal *ast = (AstGetGlobal *)calloc(1, sizeof(AstGetGlobal));
  ast->ast.type = AST_GET_GLOBAL;
  ast->symbol = name;
  return (Ast *)ast;
}

Ast *newAstBlock(size_t line, Ast *first) {
  AstBlock *ast = (AstBlock *)calloc(1, sizeof(AstBlock));
  ast->ast.type = AST_BLOCK;
  ast->first = first;
  return (Ast *)ast;
}

Ast *newAstBinop(size_t line, BinopType type, Ast *args) {
  AstBinop *ast = (AstBinop *)calloc(1, sizeof(AstBinop));
  ast->ast.type = AST_BINOP;
  ast->type = type;
  ast->args = args;
  return (Ast *)ast;
}

Ast *newAstCall(size_t line, Ast *function, Symbol *name, Ast *firstArg) {
  AstCall *ast = (AstCall *)calloc(1, sizeof(AstCall));
  ast->ast.type = AST_CALL;
  ast->function = function;
  ast->name = name;
  ast->firstArg = firstArg;
  return (Ast *)ast;
}

void freeAst(Ast *ast) {
  if (!ast) {
    return;
  }
  freeAst(ast->next);
  switch (ast->type) {
    case AST_LITERAL:
    case AST_GET_GLOBAL:
      break;
    case AST_BLOCK:
      freeAst(((AstBlock *)ast)->first);
      break;
    case AST_BINOP:
      freeAst(((AstBinop *)ast)->args);
      break;
    case AST_CALL:
      freeAst(((AstCall *)ast)->function);
      freeAst(((AstCall *)ast)->firstArg);
      break;
  }
  free(ast);
}

#include "mtots3ast.h"

#include <stdlib.h>

AstLiteral *newAstLiteral(size_t line, Value value) {
  AstLiteral *ast = (AstLiteral *)calloc(1, sizeof(AstLiteral));
  ast->ast.type = AST_LITERAL;
  ast->value = value;
  retainValue(value);
  return ast;
}

AstName *newAstName(size_t line, Symbol *name) {
  AstName *ast = (AstName *)calloc(1, sizeof(AstName));
  ast->ast.type = AST_NAME;
  ast->symbol = name;
  return ast;
}

AstBlock *newAstBlock(size_t line, Ast *first) {
  AstBlock *ast = (AstBlock *)calloc(1, sizeof(AstBlock));
  ast->ast.type = AST_BLOCK;
  ast->first = first;
  return ast;
}

AstCall *newAstCall(size_t line, Ast *function, Symbol *name, Ast *firstArg) {
  AstCall *ast = (AstCall *)calloc(1, sizeof(AstCall));
  ast->ast.type = AST_CALL;
  ast->function = function;
  ast->name = name;
  ast->firstArg = firstArg;
  return ast;
}

void freeAst(Ast *ast) {
  if (!ast) {
    return;
  }
  freeAst(ast->next);
  switch (ast->type) {
    case AST_LITERAL:
    case AST_NAME:
      break;
    case AST_BLOCK:
      freeAst(((AstBlock *)ast)->first);
      break;
    case AST_CALL:
      freeAst(((AstCall *)ast)->function);
      freeAst(((AstCall *)ast)->firstArg);
      break;
  }
  free(ast);
}

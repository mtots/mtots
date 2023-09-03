#ifndef mtots3ast_h
#define mtots3ast_h

#include "mtots2value.h"

typedef struct Ast Ast;

typedef enum AstType {
  AST_LITERAL,
  AST_NAME,
  AST_BLOCK,
  AST_CALL
} AstType;

struct Ast {
  AstType type;
  size_t line;

  /** Used for nodes that are part of a sequence */
  Ast *next;
};

typedef struct AstLiteral {
  Ast ast;
  Value value;
} AstLiteral;

typedef struct AstName {
  Ast ast;
  Symbol *symbol;
} AstName;

typedef struct AstBlock {
  Ast ast;
  Ast *first;
} AstBlock;

/** Function or method call */
typedef struct AstCall {
  Ast ast;
  Ast *function; /* receiver for method calls */
  Symbol *name;  /* NULL for functions */
  Ast *firstArg;
} AstCall;

AstLiteral *newAstLiteral(size_t line, Value value);
AstName *newAstName(size_t line, Symbol *name);
AstBlock *newAstBlock(size_t line, Ast *first);
AstCall *newAstCall(size_t line, Ast *function, Symbol *name, Ast *firstArg);
void freeAst(Ast *ast);

#endif /*mtots3ast_h*/

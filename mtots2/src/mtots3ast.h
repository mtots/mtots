#ifndef mtots3ast_h
#define mtots3ast_h

#include "mtots2value.h"

typedef struct Ast Ast;

typedef enum AstType {
  AST_LITERAL,
  AST_NAME,
  AST_BLOCK,
  AST_BINOP,
  AST_CALL
} AstType;

typedef enum BinopType {
  BINOP_ADD,
  BINOP_SUBTRACT,
  BINOP_MULTIPLY,
  BINOP_MODULO,
  BINOP_DIVIDE,
  BINOP_FLOOR_DIVIDE
} BinopType;

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

typedef struct AstBinop {
  Ast ast;
  BinopType type;
  Ast *args;
} AstBinop;

/** Function or method call */
typedef struct AstCall {
  Ast ast;
  Ast *function; /* receiver for method calls */
  Symbol *name;  /* NULL for functions */
  Ast *firstArg;
} AstCall;

Ast *newAstLiteral(size_t line, Value value);
Ast *newAstName(size_t line, Symbol *name);
Ast *newAstBlock(size_t line, Ast *first);
Ast *newAstBinop(size_t line, BinopType type, Ast *args);
Ast *newAstCall(size_t line, Ast *function, Symbol *name, Ast *firstArg);
void freeAst(Ast *ast);

#endif /*mtots3ast_h*/

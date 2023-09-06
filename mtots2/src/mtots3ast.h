#ifndef mtots3ast_h
#define mtots3ast_h

#include "mtots2value.h"

typedef struct Ast Ast;

typedef enum AstType {
  AST_LITERAL,
  AST_GET_GLOBAL,
  AST_SET_GLOBAL,
  AST_BLOCK,
  AST_UNOP,
  AST_BINOP,
  AST_LOGICAL,
  AST_CALL
} AstType;

typedef enum UnopType {
  UNOP_POSITIVE,
  UNOP_NEGATIVE
} UnopType;

typedef enum BinopType {
  BINOP_ADD,
  BINOP_SUBTRACT,
  BINOP_MULTIPLY,
  BINOP_MODULO,
  BINOP_DIVIDE,
  BINOP_FLOOR_DIVIDE
} BinopType;

typedef enum LogicalType {
  LOGICAL_NOT,
  LOGICAL_OR,
  LOGICAL_AND,
  LOGICAL_IF
} LogicalType;

struct Ast {
  AstType type;
  size_t line;

  /** Used for nodes that are part of a sequence */
  Ast *next;

#if MTOTS_DEBUG_MEMORY_LEAK
  Ast *suf; /* next */
  Ast *pre; /* prev */
#endif
};

typedef struct AstLiteral {
  Ast ast;
  Value value;
} AstLiteral;

typedef struct AstGetGlobal {
  Ast ast;
  Symbol *symbol;
} AstGetGlobal;

typedef struct AstSetGlobal {
  Ast ast;
  Symbol *symbol;
  Ast *value;
} AstSetGlobal;

typedef struct AstBlock {
  Ast ast;
  Ast *first;
} AstBlock;

typedef struct AstUnop {
  Ast ast;
  UnopType type;
  Ast *arg;
} AstUnop;

typedef struct AstBinop {
  Ast ast;
  BinopType type;
  Ast *args;
} AstBinop;

typedef struct AstLogical {
  Ast ast;
  LogicalType type;
  Ast *args;
} AstLogical;

/** Function or method call */
typedef struct AstCall {
  Ast ast;
  Symbol *name;     /* NULL for functions, otherwise method name */
  Ast *funcAndArgs; /* receiver for method calls */
} AstCall;

Ast *newAstLiteral(size_t line, Value value);
Ast *newAstGetGlobal(size_t line, Symbol *name);
Ast *newAstSetGlobal(size_t line, Symbol *name, Ast *value);
Ast *newAstBlock(size_t line, Ast *first);
Ast *newAstUnop(size_t line, UnopType type, Ast *arg);
Ast *newAstBinop(size_t line, BinopType type, Ast *args);
Ast *newAstLogical(size_t line, LogicalType type, Ast *args);
Ast *newAstCall(size_t line, Symbol *name, Ast *funcAndArgs);
void freeAst(Ast *ast);

#if MTOTS_DEBUG_MEMORY_LEAK
size_t printLeakedAsts(void);
#endif

#endif /*mtots3ast_h*/

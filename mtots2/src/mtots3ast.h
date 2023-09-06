#ifndef mtots3ast_h
#define mtots3ast_h

#include "mtots2value.h"

typedef struct Ast Ast;
typedef struct Parameter Parameter;

typedef enum AstType {
  AST_MODULE,
  AST_LITERAL,
  AST_GET_GLOBAL,
  AST_SET_GLOBAL,
  AST_GET_LOCAL,
  AST_SET_LOCAL,
  AST_BLOCK,
  AST_UNOP,
  AST_BINOP,
  AST_LOGICAL,
  AST_CALL,
  AST_FUNCTION
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

struct Parameter {
  Symbol *name;
  Value defaultValue;
  Parameter *next;
};

struct Ast {
  AstType type;
  u32 line;

  /** Used for nodes that are part of a sequence */
  Ast *next;

#if MTOTS_DEBUG_MEMORY_LEAK
  Ast *suf; /* next */
  Ast *pre; /* prev */
#endif
};

typedef struct AstModule {
  Ast ast;
  Ast *first;
} AstModule;

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

typedef struct AstGetLocal {
  Ast ast;
  Symbol *symbol;
  u16 index;
} AstGetLocal;

typedef struct AstSetLocal {
  Ast ast;
  Symbol *symbol;
  u16 index;
  Ast *value;
} AstSetLocal;

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

typedef struct AstFunction {
  Ast ast;
  Symbol *name;
  Parameter *parameters;
  i16 arity;
  i16 maxArity;
  Ast *body;
} AstFunction;

Parameter *newParameter(Symbol *name, Value defaultValue);
Ast *newAstModule(u32 line, Ast *first);
Ast *newAstLiteral(u32 line, Value value);
Ast *newAstGetGlobal(u32 line, Symbol *name);
Ast *newAstSetGlobal(u32 line, Symbol *name, Ast *value);
Ast *newAstGetLocal(u32 line, Symbol *name, u16 index);
Ast *newAstSetLocal(u32 line, Symbol *name, u16 index, Ast *value);
Ast *newAstBlock(u32 line, Ast *first);
Ast *newAstUnop(u32 line, UnopType type, Ast *arg);
Ast *newAstBinop(u32 line, BinopType type, Ast *args);
Ast *newAstLogical(u32 line, LogicalType type, Ast *args);
Ast *newAstCall(u32 line, Symbol *name, Ast *funcAndArgs);
Ast *newAstFunction(u32 line, Symbol *name, Parameter *parameters, Ast *body);
void freeParameter(Parameter *parameter);
void freeAst(Ast *ast);

#if MTOTS_DEBUG_MEMORY_LEAK
size_t printLeakedAsts(void);
#endif

#endif /*mtots3ast_h*/

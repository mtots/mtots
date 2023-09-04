#ifndef mtots4eval_h
#define mtots4eval_h

#include "mtots3ast.h"

typedef enum EvalStatus {
  EVAL_STATUS_OK,
  EVAL_STATUS_ERR,
  EVAL_STATUS_RETURN
} EvalStatus;

/** Evaluates an AST node and pushes the result on to the stack */
EvalStatus evalAst(Ast *node);

void setEvalGlobal(Symbol *name, Value value);

#endif /*mtots4eval_h*/

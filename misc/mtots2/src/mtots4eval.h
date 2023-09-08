#ifndef mtots4eval_h
#define mtots4eval_h

#include "mtots3ast.h"

void freeGlobals(void);

/** Evaluates an AST node and pushes the result on to the stack */
Status evalAst(Ast *node, Map *scope);

#endif /*mtots4eval_h*/

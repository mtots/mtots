#ifndef mtots4function_h
#define mtots4function_h

#include "mtots2object.h"
#include "mtots3ast.h"

typedef struct Function Function;

void retainFunction(Function *function);
void releaseFunction(Function *function);
Value functionValue(Function *function);
ubool isFunction(Value value);
Function *asFunction(Value value);

Function *newFunction(AstFunction *ast);

Class *getFunctionClass(void);

#endif /*mtots4function_h*/

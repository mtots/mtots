#include "mtots4function.h"

#include "mtots1err.h"
#include "mtots2native.h"
#include "mtots2string.h"
#include "mtots3ast.h"
#include "mtots4eval.h"

struct Function {
  Native native;
  Symbol *name;
  AstFunction *ast;
};

static void freeFunction(Object *object) {}

void retainFunction(Function *function) {
  retainObject((Object *)function);
}

void releaseFunction(Function *function) {
  releaseObject((Object *)function);
}

Value functionValue(Function *function) {
  return objectValue((Object *)function);
}

ubool isFunction(Value value) {
  return isNative(value) && ((Native *)value.as.object)->cls == getFunctionClass();
}

Function *asFunction(Value value) {
  if (!isFunction(value)) {
    panic("Expected Function but got %s", getValueKindName(value));
  }
  return (Function *)value.as.object;
}

Function *newFunction(AstFunction *ast) {
  Function *function = NEW_NATIVE(Function, getFunctionClass());
  function->name = ast->name;
  function->ast = ast;
  return function;
}

static Status functionCallImpl(i16 argc, Value *argv, Value *out) {
  Function *func = asFunction(argv[-1]);
  checkArity(argc, func->ast->arity, func->ast->maxArity);
  /* TODO: new function call frame */
  return evalAst(func->ast->body);
}

static CFunction functionCall = {"__call__", 0, I16_MAX, &functionCallImpl};

static Status functionReprImpl(i16 argc, Value *argv, Value *out) {
  Function *func = asFunction(argv[-1]);
  String *str = newString("");
  msprintf(str, "<function %s>", symbolChars(func->name));
  *out = stringValue(str);
  return STATUS_OK;
}

static CFunction functionRepr = {"__repr__", 0, 0, &functionReprImpl};

static CFunction *methods[] = {
    &functionCall,
    &functionRepr,
    NULL,
};

static Class FUNCTION_CLASS = {
    "Function",       /* name */
    sizeof(Function), /* size */
    NULL,             /* constructor */
    freeFunction,     /* destructor */
    NULL,             /* nativeStaticMethods */
    methods,          /* nativeInstanceMethods */
};

Class *getFunctionClass(void) {
  initStaticClass(&FUNCTION_CLASS);
  return &FUNCTION_CLASS;
}

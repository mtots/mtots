#include "mtots4function.h"

#include "mtots1err.h"
#include "mtots2map.h"
#include "mtots2native.h"
#include "mtots2string.h"
#include "mtots3ast.h"
#include "mtots4eval.h"

struct Function {
  Native native;
  Symbol *name;
  AstFunction *ast;
  Map *scope;
};

static void freeFunction(Object *object) {
  Function *func = (Function *)object;
  releaseMap(func->scope);
}

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

Function *newFunction(AstFunction *ast, Map *scope) {
  Function *function = NEW_NATIVE(Function, getFunctionClass());
  function->name = ast->name;
  function->ast = ast;
  function->scope = scope;
  retainMap(scope);
  return function;
}

static Status implCall(i16 argc, Value *argv, Value *out) {
  Function *func = asFunction(argv[-1]);
  Map *scope = newMapWithParent(func->scope);
  Parameter *param;
  i16 i = 0;
  checkArity(argc, func->ast->arity, func->ast->maxArity);
  for ((void)(i = 0), param = func->ast->parameters; param; param = param->next, i++) {
    if (i < argc) {
      mapSet(scope, symbolValue(param->name), argv[i]);
    } else if (isSentinel(param->defaultValue)) {
      panic("param->defaultValue is a sentinel when it should not be");
    } else {
      mapSet(scope, symbolValue(param->name), param->defaultValue);
    }
  }
  if (!evalAst(func->ast->body, scope)) {
    releaseMap(scope);
    return STATUS_ERR;
  }
  releaseMap(scope);
  return STATUS_OK;
}

static CFunction funcCall = {"__call__", 0, I16_MAX, &implCall};

static Status implRepr(i16 argc, Value *argv, Value *out) {
  Function *func = asFunction(argv[-1]);
  String *str = newString("");
  msprintf(str, "<function %s (scope=%p)>", symbolChars(func->name), (void *)func->scope);
  *out = stringValue(str);
  return STATUS_OK;
}

static CFunction funcRepr = {"__repr__", 0, 0, &implRepr};

static CFunction *methods[] = {
    &funcCall,
    &funcRepr,
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

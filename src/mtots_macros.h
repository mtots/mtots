#ifndef mtots_macros_h
#define mtots_macros_h

#include <string.h>

#include "mtots_macros_public.h"
#include "mtots_vm.h"

/* Helper macros for creating bindings */
#define WRAP_C_TYPE_EX(name, ctype, prefix, blackenFunc, freeFunc) \
  prefix NativeObjectDescriptor descriptor##name = {               \
      blackenFunc,                                                 \
      freeFunc,                                                    \
      sizeof(Obj##name),                                           \
      #name,                                                       \
  };                                                               \
  prefix ubool is##name(Value value) {                             \
    return getNativeObjectDescriptor(value) == &descriptor##name;  \
  }                                                                \
  prefix Value val##name(Obj##name *x) {                           \
    return valObjExplicit((Obj *)x);                               \
  }                                                                \
  prefix Obj##name *as##name(Value value) {                        \
    if (!is##name(value)) {                                        \
      panic("Expected " #name " but got %s", getKindName(value));  \
    }                                                              \
    return (Obj##name *)AS_OBJ_UNSAFE(value);                      \
  }                                                                \
  prefix Obj##name *alloc##name(void) {                            \
    Obj##name *ret = NEW_NATIVE(Obj##name, &descriptor##name);     \
    memset(&ret->handle, 0, sizeof(ret->handle));                  \
    return ret;                                                    \
  }

#define WRAP_C_TYPE_DEFAULT_STATIC_METHODS(name)                                  \
  static Status impl##name##StaticCall(i16 argc, Value *argv, Value *out) {       \
    *out = val##name(alloc##name());                                              \
    return STATUS_OK;                                                             \
  }                                                                               \
  static CFunction func##name##StaticCall = {impl##name##StaticCall, "__call__"}; \
  static CFunction *name##StaticMethods[] = {                                     \
      &func##name##StaticCall,                                                    \
      NULL,                                                                       \
  };

#define WRAP_PUBLIC_C_TYPE(name, ctype)                           \
  WRAP_C_TYPE_EX(name, ctype, MTOTS_NOTHING, nopBlacken, nopFree) \
  WRAP_C_TYPE_DEFAULT_STATIC_METHODS(name)

#define WRAP_C_TYPE(name, ctype)                           \
  typedef struct Obj##name {                               \
    ObjNative obj;                                         \
    ctype handle;                                          \
  } Obj##name;                                             \
  WRAP_C_TYPE_EX(name, ctype, static, nopBlacken, nopFree) \
  WRAP_C_TYPE_DEFAULT_STATIC_METHODS(name)

#define DEFINE_METHOD_COPY(className)                                       \
  static Status impl##className##_copy(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                        \
    Obj##className *other = as##className(argv[0]);                         \
    owner->handle = other->handle;                                          \
    return STATUS_OK;                                                       \
  }                                                                         \
  static CFunction func##className##_copy = {impl##className##_copy, "copy", 2};

#define DEFINE_FIELD_GETTER(className, fieldName, getterExpression)                   \
  static Status impl##className##_get##fieldName(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                                  \
    *out = getterExpression;                                                          \
    return STATUS_OK;                                                                 \
  }                                                                                   \
  static CFunction func##className##_get##fieldName = {                               \
      impl##className##_get##fieldName,                                               \
      "__get_" #fieldName,                                                            \
  };

#define DEFINE_FIELD_SETTER(className, fieldName, setterExpression)                   \
  static Status impl##className##_set##fieldName(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                                  \
    Value value = *out = argv[0];                                                     \
    setterExpression;                                                                 \
    return STATUS_OK;                                                                 \
  }                                                                                   \
  static CFunction func##className##_set##fieldName = {                               \
      impl##className##_set##fieldName,                                               \
      "__set_" #fieldName,                                                            \
      1,                                                                              \
  };

#define ADD_TYPE_TO_MODULE(name) \
  newNativeClass(module, &descriptor##name, name##Methods, name##StaticMethods)

#define WRAP_C_FUNCTION_EX(mtotsName, cname, minArgc, maxArgc, expression) \
  static Status impl##cname(i16 argc, Value *argv, Value *out) {           \
    expression;                                                            \
    return STATUS_OK;                                                      \
  }                                                                        \
  static CFunction func##cname = {impl##cname, #mtotsName, minArgc, maxArgc};

#define WRAP_C_FUNCTION(name, minArgc, maxArgc, expression) \
  WRAP_C_FUNCTION_EX(name, name, minArgc, maxArgc, expression)

#endif /*mtots_macros_h*/

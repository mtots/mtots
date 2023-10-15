#ifndef mtots_macros_h
#define mtots_macros_h

#include "mtots_vm.h"

/* Helper macros for creating bindings */

#define WRAP_C_TYPE(name, ctype)                                                  \
  typedef struct Obj##name {                                                      \
    ObjNative obj;                                                                \
    ctype handle;                                                                 \
  } Obj##name;                                                                    \
  static NativeObjectDescriptor descriptor##name = {                              \
      nopBlacken,                                                                 \
      nopFree,                                                                    \
      sizeof(Obj##name),                                                          \
      #name,                                                                      \
  };                                                                              \
  static ubool is##name(Value value) {                                            \
    return getNativeObjectDescriptor(value) == &descriptor##name;                 \
  }                                                                               \
  static Value val##name(Obj##name *x) {                                          \
    return valObjExplicit((Obj *)x);                                              \
  }                                                                               \
  static Obj##name *as##name(Value value) {                                       \
    if (!is##name(value)) {                                                       \
      panic("Expected " #name " but got %s", getKindName(value));                 \
    }                                                                             \
    return (Obj##name *)AS_OBJ_UNSAFE(value);                                     \
  }                                                                               \
  static Obj##name *alloc##name() {                                               \
    Obj##name *ret = NEW_NATIVE(Obj##name, &descriptor##name);                    \
    memset(&ret->handle, 0, sizeof(ret->handle));                                 \
    return ret;                                                                   \
  }                                                                               \
  static Status impl##name##StaticCall(i16 argc, Value *argv, Value *out) {       \
    *out = val##name(alloc##name());                                              \
    return STATUS_OK;                                                             \
  }                                                                               \
  static CFunction func##name##StaticCall = {impl##name##StaticCall, "__call__"}; \
  static CFunction *name##StaticMethods[] = {                                     \
      &func##name##StaticCall,                                                    \
      NULL,                                                                       \
  };

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

#define WRAP_C_FUNCTION(name, minArgc, maxArgc, expression)     \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    expression;                                                 \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, minArgc, maxArgc};

#endif /*mtots_macros_h*/

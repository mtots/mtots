#ifndef mtots_macros_h
#define mtots_macros_h

#include "mtots_vm.h"

/* Helper macros for creating bindings (not yet in use) */

#define MTOTS_POD_STRUCT(baseName, handleType) \
  typedef struct Obj##baseName {               \
    ObjNative obj;                             \
    handleType handle;                         \
  } Obj##baseName;

#define MTOTS_POD_DESCRIPTOR(baseName, blacken, free) \
  NativeObjectDescriptor descriptor##baseName = {     \
      blacken,                                        \
      free,                                           \
      sizeof(Obj##baseName),                          \
      #baseName,                                      \
  };

#define MTOTS_POD_DESCRIPTOR_STATIC(baseName, blacken, free) \
  static MTOTS_POD_DESCRIPTOR(baseName, blacken, free)

#define MTOTS_POD_VAL(baseName)           \
  Value baseName##VAL(Obj##baseName *p) { \
    return valObjExplicit((Obj *)p);      \
  }

#define MTOTS_POD_VAL_STATIC(baseName) \
  static MTOTS_POD_VAL(baseName)

#define MTOTS_POD_IS(baseName)                                        \
  ubool is##baseName(Value value) {                                   \
    return getNativeObjectDescriptor(value) == &descriptor##baseName; \
  }

#define MTOTS_POD_IS_STATIC(baseName) \
  static MTOTS_POD_IS(baseName)

#define MTOTS_POD_AS(baseName)                                        \
  Obj##baseName *as##baseName(Value value) {                          \
    if (!is##baseName(value)) {                                       \
      panic("Expected " #baseName " but got %s", getKindName(value)); \
    }                                                                 \
    return (Obj##baseName *)value.as.obj;                             \
  }

#define MTOTS_POD_AS_STATIC(baseName) \
  static MTOTS_POD_AS(baseName)

#define MTOTS_POD_NEW(baseName)                                            \
  Obj##baseName *new##baseName() {                                         \
    Obj##baseName *ret = NEW_NATIVE(Obj##baseName, &descriptor##baseName); \
    memset(&ret->handle, 0, sizeof(ret->handle));                          \
    return ret;                                                            \
  }

#define MTOTS_POD_NEW_STATIC(baseName) \
  static MTOTS_POD_NEW(baseName)

#define MTOTS_POD_METHOD_STATIC_CALL(baseName)                                  \
  static Status impl##baseName##StaticCall(i16 argc, Value *argv, Value *out) { \
    *out = baseName##Val(new##baseName());                                      \
    return STATUS_OK;                                                           \
  }                                                                             \
  static CFunction func##baseName##StaticCall = {impl##basename##StaticCall, "__call__"};

#define MTOTS_POD_STATIC_BASE(baseName, handleType)          \
  MTOTS_POD_STRUCT(baseName, handleType)                     \
  MTOTS_POD_DESCRIPTOR_STATIC(baseName, nopBlacken, nopFree) \
  MTOTS_POD_VAL_STATIC(baseName)                             \
  MTOTS_POD_IS_STATIC(baseName)                              \
  MTOTS_POD_AS_STATIC(baseName)                              \
  MTOTS_POD_NEW_STATIC(baseName)

#endif /*mtots_macros_h*/

#ifndef mtots_macros_public_h
#define mtots_macros_public_h

#include "mtots_object.h"

#define DECLARE_PUBLIC_C_TYPE_PROTOTYPES(name, ctype) \
  typedef struct Obj##name {                          \
    ObjNative obj;                                    \
    ctype handle;                                     \
  } Obj##name;                                        \
  extern NativeObjectDescriptor descriptor##name;     \
  ubool is##name(Value value);                        \
  Value val##name(Obj##name *x);                      \
  Obj##name *as##name(Value value);                   \
  Obj##name *alloc##name(void);

#endif /*mtots_macros_public_h*/

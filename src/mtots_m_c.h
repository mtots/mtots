#ifndef mtots_m_c_h
#define mtots_m_c_h

/* Native Module c */

#include "mtots_macros_public.h"
/*
typedef enum PointerType {
  POINTER_TYPE_VOID,

  POINTER_TYPE_CHAR,
  POINTER_TYPE_SHORT,
  POINTER_TYPE_INT,
  POINTER_TYPE_LONG,

  POINTER_TYPE_UNSIGNED_SHORT,
  POINTER_TYPE_UNSIGNED_INT,
  POINTER_TYPE_UNSIGNED_LONG,

  POINTER_TYPE_U8,
  POINTER_TYPE_U16,
  POINTER_TYPE_U32,
  POINTER_TYPE_U64,

  POINTER_TYPE_I8,
  POINTER_TYPE_I16,
  POINTER_TYPE_I32,
  POINTER_TYPE_I64,

  POINTER_TYPE_SIZE_T,
  POINTER_TYPE_PTRDIFF_T,

  POINTER_TYPE_FLOAT,
  POINTER_TYPE_DOUBLE
} PointerType;

typedef struct TypedPointer {
  PointerType type;
  ubool isConst;
  union {
    void *voidPointer;
    const void *constVoidPointer;
  } as;
} TypedPointer;
*/
typedef struct ConstU8Slice {
  const u8 *buffer;
  size_t count;
  Value dataOwner;
} ConstU8Slice;

/*
const char *getPointerTypeName(PointerType ptype);
size_t getPointerItemSize(PointerType ptype);

DECLARE_PUBLIC_C_TYPE_PROTOTYPES(TypedPointer, TypedPointer)
*/

DECLARE_PUBLIC_C_TYPE_PROTOTYPES(IntCell, int)
DECLARE_PUBLIC_C_TYPE_PROTOTYPES(U16Cell, u16)
DECLARE_PUBLIC_C_TYPE_PROTOTYPES(U32Cell, u32)
DECLARE_PUBLIC_C_TYPE_PROTOTYPES(ConstU8Slice, ConstU8Slice)

void addNativeModuleC(void);

#endif /*mtots_m_c_h*/

#ifndef mtots_m_c_h
#define mtots_m_c_h

/* Native Module c */

#include "mtots_macros_public.h"

typedef struct ConstU8Array {
  const u8 *buffer;
  size_t count;
  Value dataOwner;
} ConstU8Array;

DECLARE_PUBLIC_C_TYPE_PROTOTYPES(IntCell, int)
DECLARE_PUBLIC_C_TYPE_PROTOTYPES(U32Cell, u32)
DECLARE_PUBLIC_C_TYPE_PROTOTYPES(ConstU8Array, ConstU8Array)

void addNativeModuleC(void);

#endif /*mtots_m_c_h*/

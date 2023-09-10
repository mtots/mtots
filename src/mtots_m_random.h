#ifndef mtots_m_random_h
#define mtots_m_random_h

/* Native Module random */

#include "mtots_object.h"

#define isRandom(value) (getNativeObjectDescriptor(value) == &descriptorRandom)

typedef struct ObjRandom {
  ObjNative obj;
  Random handle;
} ObjRandom;

extern NativeObjectDescriptor descriptorRandom;

ObjRandom *asRandom(Value value);

void addNativeModuleRandom(void);

#endif /*mtots_m_random_h*/

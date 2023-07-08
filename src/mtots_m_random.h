#ifndef mtots_m_random_h
#define mtots_m_random_h

/* Native Module random */

#include "mtots_object.h"

#define IS_RANDOM(value) (getNativeObjectDescriptor(value) == &descriptorRandom)
#define AS_RANDOM(value) ((ObjRandom*)AS_OBJ(value))

typedef struct ObjRandom {
  ObjNative obj;
  Random handle;
} ObjRandom;

extern NativeObjectDescriptor descriptorRandom;

void addNativeModuleRandom(void);

#endif/*mtots_m_random_h*/

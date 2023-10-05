#ifndef mtots_class_sb_h
#define mtots_class_sb_h

#include "mtots_object.h"

#define isStringBuilder(value) (getNativeObjectDescriptor(value) == &descriptorStringBuilder)

typedef struct ObjStringBuilder {
  ObjNative obj;
  StringBuilder handle;
} ObjStringBuilder;

ObjStringBuilder *asStringBuilder(Value value);

void initStringBuilderClass(void);

ObjStringBuilder *newStringBuilder(void);

extern NativeObjectDescriptor descriptorStringBuilder;

#endif /*mtots_class_sb_h*/

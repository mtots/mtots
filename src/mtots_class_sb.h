#ifndef mtots_class_sb_h
#define mtots_class_sb_h

#include "mtots_object.h"

#define IS_STRING_BUILDER(value) (getNativeObjectDescriptor(value) == &descriptorStringBuilder)

typedef struct ObjStringBuilder {
  ObjNative obj;
  Buffer buf;
} ObjStringBuilder;

ObjStringBuilder *asStringBuilder(Value value);

void initStringBuilderClass(void);

ObjStringBuilder *newStringBuilder(void);

extern NativeObjectDescriptor descriptorStringBuilder;

#endif /*mtots_class_sb_h*/

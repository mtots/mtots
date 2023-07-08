#ifndef mtots_class_rect_h
#define mtots_class_rect_h

#include "mtots_object.h"

#define AS_RECT(value) ((ObjRect*)AS_OBJ(value))
#define IS_RECT(value) (getNativeObjectDescriptor(value) == &descriptorRect)

typedef struct ObjRect {
  ObjNative obj;
  Rect handle;
} ObjRect;

Value RECT_VAL(ObjRect *rect);
ObjRect *allocRect(Rect handle);

void initRectClass(void);

extern NativeObjectDescriptor descriptorRect;

#endif/*mtots_class_rect_h*/

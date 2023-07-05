#ifndef mtots_m_image_h
#define mtots_m_image_h

/* Native Module image */

#include "mtots_object.h"

#define AS_IMAGE(value) ((ObjImage*)AS_OBJ(value))
#define IS_IMAGE(value) (getNativeObjectDescriptor(value) == &descriptorImage)

typedef struct ObjImage {
  ObjNative obj;
  size_t width;
  size_t height;
  Color *pixels;
} ObjImage;

Value IMAGE_VAL(ObjImage *image);

ObjImage *newImage(size_t width, size_t height);
void addNativeModuleMediaImage();

extern NativeObjectDescriptor descriptorImage;

#endif/*mtots_m_image_h*/

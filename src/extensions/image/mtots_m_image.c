#include "mtots_m_image.h"

#include "mtots_vm.h"
#include <stdlib.h>
#include <string.h>

#define DRAW_STYLE_FILL                                    0
#define DRAW_STYLE_OUTLINE                                 1

static void freeImage(ObjNative *n) {
  ObjImage *image = (ObjImage*)n;
  FREE_ARRAY(Color, image->pixels, image->width * image->height);
}

NativeObjectDescriptor descriptorImage = {
  nopBlacken, freeImage, sizeof(ObjImage), "Image",
};

Value IMAGE_VAL(ObjImage *image) {
  return OBJ_VAL_EXPLICIT((Obj*)image);
}

/*
 * Creates a new image with given width and height
 * NOTE: the pixel data is not set to any particular value.
 * You should overwrite them or zero them out after this call.
 */
ObjImage *newImage(size_t width, size_t height) {
  Color *pixels = ALLOCATE(Color, width * height);
  ObjImage *image = NEW_NATIVE(ObjImage, &descriptorImage);
  image->width = width;
  image->height = height;
  image->pixels = pixels;
  return image;
}

static ubool implImageStaticCall(i16 argc, Value *args, Value *out) {
  ObjImage *image = newImage(AS_SIZE(args[0]), AS_SIZE(args[1]));
  memset(image->pixels, 0, sizeof(Color) * image->width * image->height);
  *out = IMAGE_VAL(image);
  return UTRUE;
}

static CFunction funcImageStaticCall = {
  implImageStaticCall, "__call__", 2, 0, argsNumbers
};

static ubool implImageGetattr(i16 argc, Value *args, Value *out) {
  ObjImage *image = AS_IMAGE(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.widthString) {
    *out = NUMBER_VAL(image->width);
  } else if (name == vm.heightString) {
    *out = NUMBER_VAL(image->height);
  } else {
    runtimeError("Field %s not found on Image", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcImageGetattr = {
  implImageGetattr, "__getattr__", 1, 0, argsStrings,
};

static ubool implImageGet(i16 argc, Value *args, Value *out) {
  ObjImage *image = AS_IMAGE(args[-1]);
  size_t row = AS_INDEX(args[0], image->height);
  size_t column = AS_INDEX(args[1], image->width);
  *out = COLOR_VAL(image->pixels[row * image->width + column]);
  return UTRUE;
}

static CFunction funcImageGet = {
  implImageGet, "get", 2, 0, argsNumbers,
};

static ubool implImageSet(i16 argc, Value *args, Value *out) {
  ObjImage *image = AS_IMAGE(args[-1]);
  size_t row = AS_INDEX(args[0], image->height);
  size_t column = AS_INDEX(args[1], image->width);
  Color color = AS_COLOR(args[2]);
  image->pixels[row * image->width + column] = color;
  return UTRUE;
}

static TypePattern argsImageSet[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcImageSet = {
  implImageSet, "set", 3, 0, argsImageSet
};

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *staticImageMethods[] = {
    &funcImageStaticCall,
    NULL,
  };
  CFunction *imageMethods[] = {
    &funcImageGetattr,
    &funcImageGet,
    &funcImageSet,
    NULL,
  };

  newNativeClass(module, &descriptorImage, imageMethods, staticImageMethods);

  return UTRUE;
}

static CFunction func = { impl, "media.image", 1 };

void addNativeModuleMediaImage() {
  addNativeModule(&func);
}

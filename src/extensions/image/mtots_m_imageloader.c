#include "mtots_m_imageloader.h"

#include "mtotsa_stbimage.h"
#include "mtots_vm.h"

#include <string.h>

static ubool dataToImage(
    u8 *data, int width, int height, ObjImage **out) {
  ObjImage *image = newImage((size_t)width, (size_t)height);
  size_t bytelen = image->width * image->height * 4;
  memcpy(image->pixels, data, bytelen);
  *out = image;
  mtotsa_free_image_data(data);
  return UTRUE;
}

static ubool rawDataToImage(const u8 *fileData, size_t len, ObjImage **out) {
  u8 *pixelData;
  int width, height;
  if (!mtotsa_load_image_from_memory(
      fileData, len, &pixelData, &width, &height)) {
    return UFALSE;
  }
  return dataToImage(pixelData, width, height, out);
}

static ubool loadImageWithBuffer(ObjDataSource *ds, ObjImage **out) {
  Buffer buffer;
  ubool status;
  if (!dataSourceInitBuffer(ds, &buffer)) {
    return UFALSE;
  }
  status = rawDataToImage(buffer.data, buffer.length, out);
  freeBuffer(&buffer);
  return status;
}

static ubool loadImageFromFile(const char *filePath, ObjImage **out) {
  u8 *pixelData;
  int width, height;
  if (!mtotsa_load_image_from_file(filePath, &pixelData, &width, &height)) {
    return UFALSE;
  }
  return dataToImage(pixelData, width, height, out);
}

ubool loadImage(ObjDataSource *ds, ObjImage **out) {
  ubool status;
  ubool gcPause;

  LOCAL_GC_PAUSE(gcPause);

  switch (ds->type) {
    case DATA_SOURCE_FILE:
      status = loadImageFromFile(ds->as.file.path->chars, out);
      break;
    default:
      status = loadImageWithBuffer(ds, out);
      break;
  }

  LOCAL_GC_UNPAUSE(gcPause);

  return status;
}

static ubool implLoadImage(i16 argc, Value *args, Value *out) {
  ObjDataSource *ds = AS_DATA_SOURCE(args[0]);
  ObjImage *image;
  if (!loadImage(ds, &image)) {
    return UFALSE;
  }
  *out = IMAGE_VAL(image);
  return UTRUE;
}

static TypePattern argsLoadImage[] = {
  { TYPE_PATTERN_NATIVE, &descriptorDataSource },
};

static CFunction funcLoadImage = {
  implLoadImage, "loadImage", 1, 0, argsLoadImage
};

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
    &funcLoadImage,
    NULL,
  };

  moduleAddFunctions(module, functions);

  return UTRUE;
}

static CFunction func = { impl, "media.image.loader", 1 };

void addNativeModuleMediaImageLoader() {
  addNativeModule(&func);
}

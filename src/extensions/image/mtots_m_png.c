#include "mtots_m_png.h"

#include "mtots_vm.h"
#include "lodepng.h"
#include <stdlib.h>

ubool loadPNGImage(ObjDataSource *ds, ObjImage **out) {
  u8 *imageData;
  u32 width, height;
  ObjImage *image;
  unsigned int status;

  switch (ds->type) {
    case DATA_SOURCE_FILE:
      status = lodepng_decode32_file(&imageData, &width, &height, ds->as.file.path->chars);
      break;
    default: {
      /* For unknown types, use an intermediate buffer */
      Buffer buffer;
      dataSourceInitBuffer(ds, &buffer);
      status = lodepng_decode32(
        &imageData, &width, &height, buffer.data, buffer.length);
      freeBuffer(&buffer);
      break;
    }
  }

  if (status != 0) {
    runtimeError("lodepng: %s", lodepng_error_text(status));
    return UFALSE;
  }

  image = newImage(width, height);
  memcpy(image->pixels, imageData, 4 * width * height);
  free(imageData);

  *out = image;

  return UTRUE;
}

static ubool savePNGImageToFile(const char *filePath, ObjImage *image) {
  unsigned int status = lodepng_encode32_file(
    filePath, (u8*)image->pixels, image->width, image->height);
  if (status != 0) {
    runtimeError("lodepng: %s", lodepng_error_text(status));
    return UFALSE;
  }
  return UTRUE;
}

static ubool savePNGImageToBuffer(Buffer *buffer, ObjImage *image) {
  u8 *pngData;
  size_t pngDataSize;
  unsigned int status = lodepng_encode32(
    &pngData, &pngDataSize, (u8*)image->pixels, image->width, image->height);
  if (status != 0) {
    runtimeError("lodepng: %s", lodepng_error_text(status));
    return UFALSE;
  }
  bufferAddBytes(buffer, pngData, pngDataSize);
  return UTRUE;
}

ubool savePNGImage(ObjDataSink *ds, ObjImage *image) {
  switch (ds->type) {
    case DATA_SINK_BUFFER:
      return savePNGImageToBuffer(&ds->as.buffer->handle, image);
    case DATA_SINK_FILE:
      return savePNGImageToFile(ds->as.file.path->chars, image);
    default:break;
  }
  /* Unknown sink type. In this case, try writing to a temporary buffer
   * and write out that value */
  {
    Buffer buffer;
    initBuffer(&buffer);
    if (!savePNGImageToBuffer(&buffer, image)) {
      freeBuffer(&buffer);
      return UFALSE;
    }
    if (!dataSinkWriteBytes(ds, buffer.data, buffer.length)) {
      freeBuffer(&buffer);
      return UFALSE;
    }
    freeBuffer(&buffer);
  }
  return UTRUE;
}

static ubool implLoadPNG(i16 argc, Value *args, Value *out) {
  ObjDataSource *ds = AS_DATA_SOURCE(args[0]);
  ObjImage *image;
  if (!loadPNGImage(ds, &image)) {
    return UFALSE;
  }
  *out = IMAGE_VAL(image);
  return UTRUE;
}

static TypePattern argsLoadPNG[] = {
  { TYPE_PATTERN_NATIVE, &descriptorDataSource },
};

static CFunction funcLoadPNG = { implLoadPNG, "loadPNG", 1, 0, argsLoadPNG };

static ubool implSavePNG(i16 argc, Value *args, Value *out) {
  ObjDataSink *ds = AS_DATA_SINK(args[0]);
  ObjImage *image = AS_IMAGE(args[1]);
  return savePNGImage(ds, image);
}

static TypePattern argsSavePNG[] = {
  { TYPE_PATTERN_NATIVE, &descriptorDataSink },
  { TYPE_PATTERN_NATIVE, &descriptorImage },
};

static CFunction funcSavePNG = { implSavePNG, "savePNG", 2, 0, argsSavePNG };

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
    &funcLoadPNG,
    &funcSavePNG,
    NULL,
  };

  moduleAddFunctions(module, functions);

  return UTRUE;
}

static CFunction func = { impl, "media.png", 1 };

void addNativeModuleMediaPNG(void) {
  addNativeModule(&func);
}

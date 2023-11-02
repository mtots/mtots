#include "mtots_m_xlodepng.h"

#if MTOTS_ENABLE_LODEPNG
#include <stdlib.h>

#include "lodepng.h"
#include "mtots.h"

#define ADD_CONST_INT(name, val) \
  mapSetN(&module->fields, (name), valNumber((val)))

static Status lodepngError(const char *functionName, unsigned errorcode) {
  return runtimeError("%s: %s", functionName, lodepng_error_text(errorcode));
}

static size_t getBytesPerPixel(LodePNGColorType colortype, unsigned bitdepth) {
  if (bitdepth == 8) {
    switch (colortype) {
      case LCT_RGB:
        return 3;
      case LCT_RGBA:
        return 4;
      default:
        break;
    }
  }
  panic("Could not determine bytes per pixel (colortype = %d, bitdepth = %u)",
        colortype, bitdepth);
}

static Status impldecode_memory(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  unsigned *w = asUnsignedIntPointer(argv[1]);
  unsigned *h = asUnsignedIntPointer(argv[2]);
  const u8 *inp = asConstU8Pointer(argv[3]);
  size_t inSize = asSize(argv[4]);
  LodePNGColorType colortype = (LodePNGColorType)(int)asInt(argv[5]);
  unsigned bitdepth = asUnsigned(argv[6]);
  u8 *outPtr;
  unsigned errorcode = lodepng_decode_memory(&outPtr, w, h, inp, inSize, colortype, bitdepth);
  if (errorcode != 0) {
    return lodepngError("lodepng_decode_memory", errorcode);
  }
  bufferAddBytes(outb, outPtr, getBytesPerPixel(colortype, bitdepth) * *w * *h);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcdecode_memory = {impldecode_memory, "decode_memory", 7};

static Status impldecode32(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  unsigned *w = asUnsignedIntPointer(argv[1]);
  unsigned *h = asUnsignedIntPointer(argv[2]);
  const u8 *inp = asConstU8Pointer(argv[3]);
  size_t inSize = asSize(argv[4]);
  u8 *outPtr;
  unsigned errorcode = lodepng_decode32(&outPtr, w, h, inp, inSize);
  if (errorcode != 0) {
    return lodepngError("lodepng_decode32", errorcode);
  }
  bufferAddBytes(outb, outPtr, getBytesPerPixel(LCT_RGBA, 8) * *w * *h);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcdecode32 = {impldecode32, "decode32", 5};

static Status impldecode24(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  unsigned *w = asUnsignedIntPointer(argv[1]);
  unsigned *h = asUnsignedIntPointer(argv[2]);
  const u8 *inp = asConstU8Pointer(argv[3]);
  size_t inSize = asSize(argv[4]);
  u8 *outPtr;
  unsigned errorcode = lodepng_decode24(&outPtr, w, h, inp, inSize);
  if (errorcode != 0) {
    return lodepngError("lodepng_decode24", errorcode);
  }
  bufferAddBytes(outb, outPtr, getBytesPerPixel(LCT_RGB, 8) * *w * *h);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcdecode24 = {impldecode24, "decode24", 5};

static Status impldecode_file(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  unsigned *w = asUnsignedIntPointer(argv[1]);
  unsigned *h = asUnsignedIntPointer(argv[2]);
  const char *filename = asString(argv[3])->chars;
  LodePNGColorType colortype = (LodePNGColorType)(int)asInt(argv[4]);
  unsigned bitdepth = asUnsigned(argv[5]);
  u8 *outPtr;
  unsigned errorcode = lodepng_decode_file(&outPtr, w, h, filename, colortype, bitdepth);
  if (errorcode != 0) {
    return lodepngError("lodepng_decode_file", errorcode);
  }
  bufferAddBytes(outb, outPtr, getBytesPerPixel(colortype, bitdepth) * *w * *h);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcdecode_file = {impldecode_file, "decode_file", 6};

static Status impldecode32_file(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  unsigned *w = asUnsignedIntPointer(argv[1]);
  unsigned *h = asUnsignedIntPointer(argv[2]);
  const char *filename = asString(argv[3])->chars;
  u8 *outPtr;
  unsigned errorcode = lodepng_decode32_file(&outPtr, w, h, filename);
  if (errorcode != 0) {
    return lodepngError("lodepng_decode32_file", errorcode);
  }
  bufferAddBytes(outb, outPtr, getBytesPerPixel(LCT_RGBA, 8) * *w * *h);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcdecode32_file = {impldecode32_file, "decode32_file", 4};

static Status impldecode24_file(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  unsigned *w = asUnsignedIntPointer(argv[1]);
  unsigned *h = asUnsignedIntPointer(argv[2]);
  const char *filename = asString(argv[3])->chars;
  u8 *outPtr;
  unsigned errorcode = lodepng_decode24_file(&outPtr, w, h, filename);
  if (errorcode != 0) {
    return lodepngError("lodepng_decode24_file", errorcode);
  }
  bufferAddBytes(outb, outPtr, getBytesPerPixel(LCT_RGB, 8) * *w * *h);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcdecode24_file = {impldecode24_file, "decode24_file", 4};

static Status implencode_memory(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  const u8 *image = asConstU8Pointer(argv[1]);
  unsigned w = asUnsigned(argv[2]);
  unsigned h = asUnsigned(argv[3]);
  LodePNGColorType colortype = (LodePNGColorType)(int)asInt(argv[4]);
  unsigned bitdepth = asUnsigned(argv[5]);
  u8 *outPtr;
  size_t outSize;
  unsigned errorcode = lodepng_encode_memory(&outPtr, &outSize, image, w, h, colortype, bitdepth);
  if (errorcode != 0) {
    return lodepngError("encode_memory", errorcode);
  }
  bufferAddBytes(outb, outPtr, outSize);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcencode_memory = {implencode_memory, "encode_memory", 6};

static Status implencode32(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  const u8 *image = asConstU8Pointer(argv[1]);
  unsigned w = asUnsigned(argv[2]);
  unsigned h = asUnsigned(argv[3]);
  u8 *outPtr;
  size_t outSize;
  unsigned errorcode = lodepng_encode32(&outPtr, &outSize, image, w, h);
  if (errorcode != 0) {
    return lodepngError("encode32", errorcode);
  }
  bufferAddBytes(outb, outPtr, outSize);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcencode32 = {implencode32, "encode32", 4};

static Status implencode24(i16 argc, Value *argv, Value *out) {
  Buffer *outb = &asBuffer(argv[0])->handle;
  const u8 *image = asConstU8Pointer(argv[1]);
  unsigned w = asUnsigned(argv[2]);
  unsigned h = asUnsigned(argv[3]);
  u8 *outPtr;
  size_t outSize;
  unsigned errorcode = lodepng_encode24(&outPtr, &outSize, image, w, h);
  if (errorcode != 0) {
    return lodepngError("encode24", errorcode);
  }
  bufferAddBytes(outb, outPtr, outSize);
  free(outPtr);
  return STATUS_OK;
}

static CFunction funcencode24 = {implencode24, "encode24", 4};

static Status implencode_file(i16 argc, Value *argv, Value *out) {
  const char *filename = asString(argv[0])->chars;
  const u8 *image = asConstU8Pointer(argv[1]);
  unsigned w = asUnsigned(argv[2]);
  unsigned h = asUnsigned(argv[3]);
  LodePNGColorType colortype = (LodePNGColorType)(int)asInt(argv[4]);
  unsigned bitdepth = asUnsigned(argv[5]);
  unsigned errorcode = lodepng_encode_file(filename, image, w, h, colortype, bitdepth);
  if (errorcode != 0) {
    return lodepngError("encode32_file", errorcode);
  }
  return STATUS_OK;
}

static CFunction funcencode_file = {implencode_file, "encode_file", 6};

static Status implencode32_file(i16 argc, Value *argv, Value *out) {
  const char *filename = asString(argv[0])->chars;
  const u8 *image = asConstU8Pointer(argv[1]);
  unsigned w = asUnsigned(argv[2]);
  unsigned h = asUnsigned(argv[3]);
  unsigned errorcode = lodepng_encode32_file(filename, image, w, h);
  if (errorcode != 0) {
    return lodepngError("lodepng_encode32_file", errorcode);
  }
  return STATUS_OK;
}

static CFunction funcencode32_file = {implencode32_file, "encode32_file", 4};

static Status implencode24_file(i16 argc, Value *argv, Value *out) {
  const char *filename = asString(argv[0])->chars;
  const u8 *image = asConstU8Pointer(argv[1]);
  unsigned w = asUnsigned(argv[2]);
  unsigned h = asUnsigned(argv[3]);
  unsigned errorcode = lodepng_encode24_file(filename, image, w, h);
  if (errorcode != 0) {
    return lodepngError("lodepng_encode24_file", errorcode);
  }
  return STATUS_OK;
}

static CFunction funcencode24_file = {implencode24_file, "encode24_file", 4};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcdecode_memory,
      &funcdecode32,
      &funcdecode24,
      &funcdecode_file,
      &funcdecode32_file,
      &funcdecode24_file,
      &funcencode_memory,
      &funcencode32,
      &funcencode24,
      &funcencode_file,
      &funcencode32_file,
      &funcencode24_file,
      NULL,
  };

  moduleAddFunctions(module, functions);

  ADD_CONST_INT("LCT_GREY", LCT_GREY);
  ADD_CONST_INT("LCT_RGB", LCT_RGB);
  ADD_CONST_INT("LCT_PALETTE", LCT_PALETTE);
  ADD_CONST_INT("LCT_GREY_ALPHA", LCT_GREY_ALPHA);
  ADD_CONST_INT("LCT_RGBA", LCT_RGBA);
  ADD_CONST_INT("LCT_MAX_OCTET_VALUE", LCT_MAX_OCTET_VALUE);

  return STATUS_OK;
}
static CFunction func = {impl, "x.lodepng", 1};

void addNativeModuleXLodepng(void) {
  addNativeModule(&func);
}

#else
void addNativeModuleXLodepng(void) {}
#endif

#ifndef mtots_m_paco_bmp_h
#define mtots_m_paco_bmp_h

#include "mtots_m_paco_sdl.h"
#include "mtots_m_paco_ss.h"

#include <string.h>

#define BMP_FILE_HEADER_SIZE   14
#define BMP_DIB_HEADER_SIZE    40
#define BMP_COLOR_TABLE_SIZE   (256*4)
#define BMP_PIXEL_START_POS    (BMP_FILE_HEADER_SIZE + BMP_DIB_HEADER_SIZE + BMP_COLOR_TABLE_SIZE)
#define BMP_FILE_SIZE(w,h)     (BMP_PIXEL_START_POS + (w)*(h))
#define DEFAULT_SPRITE_WIDTH    8
#define DEFAULT_SPRITE_HEIGHT   8

static ubool saveSpriteSheetToBuffer(ObjSpriteSheet *sheet, Buffer *out) {
  /* TODO: Currently we lose spriteWidth and spriteHeight information.
   * Consider a format where we store this in the file somewhere. */
  bufferSetMinCapacity(out, BMP_FILE_SIZE(sheet->width, sheet->height));
  bufferLock(out);

  /* file header */
  bufferAddU8(out, 'B');
  bufferAddU8(out, 'M');
  bufferAddU32(out, BMP_FILE_SIZE(sheet->width, sheet->height));
  bufferAddU16(out, 0); /* reserved */
  bufferAddU16(out, 0); /* reserved */
  bufferAddU32(out, BMP_PIXEL_START_POS);

  /* DIB header */
  bufferAddU32(out, BMP_DIB_HEADER_SIZE);
  bufferAddU32(out, sheet->width);
  bufferAddU32(out, sheet->height);
  bufferAddU16(out, 1); /* number of color planes, must be 1 */
  bufferAddU16(out, 8); /* bits per pixel */
  bufferAddU32(out, 0); /* compression method (none) */
  bufferAddU32(out, 0); /* raw image size (may be 0 if no compression) */
  bufferAddU32(out, 0); /* horizontal resolution */
  bufferAddU32(out, 0); /* vertical resolution */
  bufferAddU32(out, 0); /* the number of colors (0 to default to 2^n) */
  bufferAddU32(out, 0); /* important colors (generally ignored) */

  /* Color Table */
  {
    size_t i = 0;
    for (; i < sizeof(systemColors)/sizeof(SDL_Color); i++) {
      bufferAddU8(out, systemColors[i].b);
      bufferAddU8(out, systemColors[i].g);
      bufferAddU8(out, systemColors[i].r);
      bufferAddU8(out, 0);
    }
    for (; i < 256; i++) {
      bufferAddU32(out, 0);
    }
  }

  /* Pixel Data */
  {
    size_t y;
    u8 *outPixelData = out->data + out->length;
    bufferSetLength(out, out->length + (sheet->width * sheet->height));
    for (y = 0; y < sheet->height; y++) {
      memcpy(
        outPixelData + y * sheet->width,
        sheet->pixels + ((sheet->height - 1 - y) * sheet->width),
        sheet->width);
    }
  }

  if (out->length != BMP_FILE_SIZE(sheet->width, sheet->height)) {
    panic(
      "INTERNAL ERROR: sprite sheet written bytes unexpected size (expected %lu, got %lu)",
      (unsigned long)BMP_FILE_SIZE(sheet->width, sheet->height),
      (unsigned long)out->length);
  }

  return UTRUE;
}

static ubool saveSpriteSheetToFile(ObjSpriteSheet *sheet, const char *fileName) {
  Buffer buffer;
  FILE *file;
  initBuffer(&buffer);
  if (!saveSpriteSheetToBuffer(sheet, &buffer)) {
    return UFALSE;
  }
  file = fopen(fileName, "wb");
  if (!file) {
    freeBuffer(&buffer);
    runtimeError("Faield to open file (wb) %s", fileName);
    return UFALSE;
  }
  if (fwrite(buffer.data, 1, buffer.length, file) != buffer.length) {
    freeBuffer(&buffer);
    fclose(file);
    runtimeError("Failed to write to file %s", fileName);
    return UFALSE;
  }
  fclose(file);
  freeBuffer(&buffer);
  return UTRUE;
}

static ubool loadSpriteSheetFromBuffer(
    Buffer *buffer,
    size_t spriteWidth,
    size_t spriteHeight,
    ObjSpriteSheet **out) {
  size_t fileSize, width, height;
  ObjSpriteSheet *sheet;

  if (buffer->length < BMP_FILE_HEADER_SIZE) {
    runtimeError("buffer too shrot to be a bitmap");
    return UFALSE;
  }

  fileSize = bufferGetU32(buffer, 2);
  if (buffer->length != fileSize) {
    runtimeError("Bitmap file size does not meet expected file size");
    return UFALSE;
  }

  if (bufferGetI32(buffer, 14) != 40) {
    runtimeError("Unsupported bitmap header type");
    return UFALSE;
  }

  width = bufferGetI32(buffer, 18);
  height = bufferGetI32(buffer, 22);

  if (bufferGetI16(buffer, 28) != 8) {
    runtimeError(
      "Unsupported bits per pixel (must be 8, got %d)",
      bufferGetI16(buffer, 28));
    return UFALSE;
  }
  if (bufferGetI32(buffer, 30) != 0) {
    runtimeError(
      "Unsupported compression method (must be 0, got %d)",
      bufferGetI32(buffer, 30));
    return UFALSE;
  }
  if (bufferGetI32(buffer, 46) != 0 && bufferGetI32(buffer, 46) != 256) {
    runtimeError(
      "Unsupported palette size (must be 0 or 256, got %d)",
      bufferGetI32(buffer, 46));
    return UFALSE;
  }

  if (!newSpriteSheet(width, height, spriteWidth, spriteHeight, &sheet)) {
    return UFALSE;
  }

  {
    size_t y;
    for (y = 0; y < height; y++) {
      memcpy(
        sheet->pixels + (height - 1 - y) * width,
        buffer->data + BMP_PIXEL_START_POS + y * width,
        width);
    }
  }

  *out = sheet;
  return UTRUE;
}

static ubool loadSpriteSheetFromFile(
    const char *fileName,
    size_t spriteWidth,
    size_t spriteHeight,
    ObjSpriteSheet **out) {
  ubool status = UFALSE;
  size_t fileSize;
  Buffer buffer;
  FILE *file;

  initBuffer(&buffer);
  file = fopen(fileName, "rb");
  if (!file) {
    runtimeError("Failed to open file (mode=rb) %s", fileName);
    goto exit;
  }

  bufferSetLength(&buffer, BMP_FILE_HEADER_SIZE);
  if (fread(buffer.data, 1, BMP_FILE_HEADER_SIZE, file) != BMP_FILE_HEADER_SIZE) {
    runtimeError("File too short to be a paco sprite sheet");
    goto exit;
  }
  if (buffer.data[0] != 'B' || buffer.data[1] != 'M') {
    runtimeError("Missing bitmap magic - cannot be a paco sprite sheet");
    goto exit;
  }
  if (bufferGetI32(&buffer, 10) != BMP_PIXEL_START_POS) {
    runtimeError("Unsupported bitmap layout");
    goto exit;
  }
  fileSize = bufferGetU32(&buffer, 2);

  bufferSetLength(&buffer, fileSize);
  if (fread(buffer.data + BMP_FILE_HEADER_SIZE, 1, fileSize - BMP_FILE_HEADER_SIZE, file) !=
      fileSize - BMP_FILE_HEADER_SIZE) {
    runtimeError("Not enough bytes in BMP file");
    goto exit;
  }

  fclose(file);
  file = NULL;

  if (!loadSpriteSheetFromBuffer(&buffer, spriteWidth, spriteHeight, out)) {
    goto exit;
  }

  status = UTRUE;
exit:
  if (file) {
    fclose(file);
  }
  freeBuffer(&buffer);
  return status;
}

#endif/*mtots_m_paco_bmp_h*/

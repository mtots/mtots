/**
 * We assume that our platform is little endian.
 */

#include "mtots_util_buffer.h"

#include "mtots_util_error.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/**
 * For now, we assume that we're always little endian.
 */
#define PLATFORM_BYTE_ORDER MTOTS_LITTLE_ENDIAN

/** Reverse the bytes if the given byte order does not match
 * the platform byte order */
static void maybeReverseBytes(
    u8 *data, size_t length, ByteOrder byteOrder) {
  if (byteOrder != PLATFORM_BYTE_ORDER) {
    size_t i, j;
    if (length) {
      for (i = 0, j = length - 1; i < j; i++, j--) {
        u8 tmp = data[i];
        data[i] = data[j];
        data[j] = tmp;
      }
    }
  }
}

static void checkIndex(Buffer *buf, size_t pos, size_t length) {
  if (pos + length < pos || pos + length > buf->length) {
    panic(
      "Buffer index out of bounds (buflen = %lu, pos = %lu, length = %lu)",
      (unsigned long)buf->length,
      (unsigned long)pos,
      (unsigned long)length);
  }
}

static void addByte(Buffer *buf, u8 byte) {
  bufferSetMinCapacity(buf, buf->length + 2);
  buf->data[buf->length++] = byte;
  buf->data[buf->length] = '\0';
}

static void addBytesWithByteOrder(Buffer *buf, u8 *bytes, size_t length) {
  u8 *start = buf->data + buf->length;
  bufferAddBytes(buf, bytes, length);
  maybeReverseBytes(start, length, buf->byteOrder);
}

static void getDataWithByteOrder(Buffer *buf, size_t pos, void *dest, size_t length) {
  checkIndex(buf, pos, length);
  memcpy(dest, buf->data + pos, length);
  maybeReverseBytes((u8*)dest, length, buf->byteOrder);
}

static void setDataWithByteOrder(Buffer *buf, size_t pos, void *src, size_t length) {
  bufferSetBytes(buf, pos, src, length);
  maybeReverseBytes(buf->data + pos, length, buf->byteOrder);
}

void initBuffer(Buffer *buf) {
  buf->data = NULL;
  buf->length = buf->capacity = 0;
  buf->byteOrder = MTOTS_LITTLE_ENDIAN;
  buf->isLocked = UFALSE;
  buf->ownsData = UTRUE;
}

void initBufferWithExternalData(Buffer *buf, u8 *data, size_t length) {
  buf->data = data;
  buf->length = buf->capacity = length;
  buf->byteOrder = MTOTS_LITTLE_ENDIAN;
  buf->isLocked = UTRUE;
  buf->ownsData = UFALSE;
}

void bufferLock(Buffer *buf) {
  buf->isLocked = UTRUE;
}

/* Tries to reserve the given capacity in the buffer.
 * Panics if there is not enough memory */
void bufferSetMinCapacity(Buffer *buf, size_t minCap) {
  if (buf->capacity < minCap) {
    u8 *data;
    size_t newCap = buf->capacity < 8 ? 8 : 2 * buf->capacity;
    while (newCap < minCap) {
      newCap *= 2;
    }
    if (buf->isLocked) {
      panic("Cannot increase the capacity of a locked Buffer");
    }
    data = (u8*)realloc(buf->data, newCap);
    if (data == NULL) {
      panic("Buffer: out of memory");
    }
    buf->data = data;
    buf->capacity = newCap;
  }
}

void bufferSetLength(Buffer *buf, size_t newLength) {
  size_t oldLength = buf->length;
  if (oldLength < newLength) {
    bufferSetMinCapacity(buf, newLength + 1);
    memset(buf->data + oldLength, 0, newLength - oldLength);
    buf->data[newLength] = '\0';
  }
  buf->length = newLength;
}

void bufferClear(Buffer *buf) {
  buf->length = 0;
  if (buf->capacity) {
    buf->data[0] = '\0';
  }
}

void freeBuffer(Buffer *buf) {
  if (buf->ownsData) {
    free(buf->data);
  }
}

u8 bufferGetU8(Buffer *buf, size_t pos) {
  checkIndex(buf, pos, 1);
  return buf->data[pos];
}

u16 bufferGetU16(Buffer *buf, size_t pos) {
  u16 value;
  getDataWithByteOrder(buf, pos, (void*)&value, 2);
  return value;
}

u32 bufferGetU32(Buffer *buf, size_t pos) {
  u32 value;
  getDataWithByteOrder(buf, pos, (void*)&value, 4);
  return value;
}

i8 bufferGetI8(Buffer *buf, size_t pos) {
  checkIndex(buf, pos, 1);
  return (i8)buf->data[pos];
}

i16 bufferGetI16(Buffer *buf, size_t pos) {
  i16 value;
  getDataWithByteOrder(buf, pos, (void*)&value, 2);
  return value;
}

i32 bufferGetI32(Buffer *buf, size_t pos) {
  i32 value;
  getDataWithByteOrder(buf, pos, (void*)&value, 4);
  return value;
}

f32 bufferGetF32(Buffer *buf, size_t pos) {
  f32 value;
  getDataWithByteOrder(buf, pos, (void*)&value, 4);
  return value;
}

f64 bufferGetF64(Buffer *buf, size_t pos) {
  f64 value;
  getDataWithByteOrder(buf, pos, (void*)&value, 8);
  return value;
}

void bufferSetU8(Buffer *buf, size_t pos, u8 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 1);
}

void bufferSetU16(Buffer *buf, size_t pos, u16 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 2);
}

void bufferSetU32(Buffer *buf, size_t pos, u32 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 4);
}

void bufferSetI8(Buffer *buf, size_t pos, i8 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 1);
}

void bufferSetI16(Buffer *buf, size_t pos, i16 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 2);
}

void bufferSetI32(Buffer *buf, size_t pos, i32 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 4);
}

void bufferSetF32(Buffer *buf, size_t pos, f32 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 4);
}

void bufferSetF64(Buffer *buf, size_t pos, f64 value) {
  setDataWithByteOrder(buf, pos, (void*)&value, 8);
}

void bufferSetBytes(Buffer *buf, size_t pos, void *data, size_t length) {
  checkIndex(buf, pos, length);
  memcpy((void*)(buf->data + pos), data, length);
}

void bufferAddU8(Buffer *buf, u8 value) {
  addByte(buf, value);
}

void bufferAddU16(Buffer *buf, u16 value) {
  addBytesWithByteOrder(buf, (u8*)(void*)&value, 2);
}

void bufferAddU32(Buffer *buf, u32 value) {
  addBytesWithByteOrder(buf, (u8*)(void*)&value, 4);
}

void bufferAddI8(Buffer *buf, i8 value) {
  addByte(buf, (u8)value);
}

void bufferAddI16(Buffer *buf, i16 value) {
  addBytesWithByteOrder(buf, (u8*)(void*)&value, 2);
}

void bufferAddI32(Buffer *buf, i32 value) {
  addBytesWithByteOrder(buf, (u8*)(void*)&value, 4);
}

void bufferAddF32(Buffer *buf, f32 value) {
  addBytesWithByteOrder(buf, (u8*)(void*)&value, 4);
}

void bufferAddF64(Buffer *buf, f64 value) {
  addBytesWithByteOrder(buf, (u8*)(void*)&value, 8);
}

void bufferAddBytes(Buffer *buf, const void *data, size_t length) {
  bufferSetMinCapacity(buf, buf->length + length + 1);
  memcpy(buf->data + buf->length, data, length);
  buf->length += length;
  buf->data[buf->length] = '\0';
}

String *bufferToString(Buffer *buf) {
  return internString((char*)buf->data, buf->length);
}

void bputnumber(Buffer *buf, double number) {
  /* Trim the trailing zeros in the number representation */
  bprintf(buf, "%f", number);
  while (buf->length > 0 && buf->data[buf->length - 1] == '0') {
    buf->data[--buf->length] = '\0';
  }
  if (buf->length > 0 && buf->data[buf->length - 1] == '.') {
    buf->data[--buf->length] = '\0';
  }
}

void bputchar(Buffer *buf, char ch) {
  bufferAddU8(buf, (u8)ch);
}

void bputstrlen(Buffer *buf, const char *chars, size_t byteLength) {
  bufferAddBytes(buf, (void*)chars, byteLength);
}

void bputstr(Buffer *buf, const char *string) {
  bputstrlen(buf, string, strlen(string));
}

void bprintf(Buffer *buf, const char *format, ...) {
  va_list args;
  int intSize;
  size_t size;

  va_start(args, format);
  intSize = vsnprintf(NULL, 0, format, args);
  va_end(args);

  if (intSize < 0) {
    panic("bprintf: encoding error");
  }

  size = (size_t)intSize;

  bufferSetMinCapacity(buf, buf->length + size + 1);

  va_start(args, format);
  vsnprintf((char*)(buf->data + buf->length), size + 1, format, args);
  va_end(args);

  buf->length += size;
}

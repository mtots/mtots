#ifndef mtots_util_buffer_h
#define mtots_util_buffer_h

#include "mtots_common.h"
#include "mtots_util_string.h"

typedef enum ByteOrder {
  MTOTS_LITTLE_ENDIAN,
  MTOTS_BIG_ENDIAN
} ByteOrder;

typedef struct Buffer {
  u8 *data;
  size_t length;   /* readonly outside of mtots_util_buffer.c */
  size_t capacity; /* readonly outside of mtots_util_buffer.c */
  ByteOrder byteOrder;

  /**
   * When a Buffer is locked, it may no longer grow in size.
   * It can still mutate values with bufferSet* methods.
   *
   * NOTE: A locked Buffer should never be unlocked. There may be
   * entities holding pointers to the raw data assuming that
   * the Buffer is locked.
   */
  ubool isLocked;

  /**
   * If true, when this Buffer is freed, its data field will also be freed.
   * Otherwise, it is up to the caller who initialized the Buffer to ensure
   * that the underlying data array outlives this Buffer.
   */
  ubool ownsData;
} Buffer;

void initBuffer(Buffer *buf);
void initBufferWithExternalData(Buffer *buf, u8 *data, size_t length);
void freeBuffer(Buffer *buf);
void bufferLock(Buffer *buf);
void bufferSetMinCapacity(Buffer *buf, size_t minCap);
void bufferSetLength(Buffer *buf, size_t newLength);
void bufferClear(Buffer *buf);
u8 bufferGetU8(Buffer *buf, size_t pos);
u16 bufferGetU16(Buffer *buf, size_t pos);
u32 bufferGetU32(Buffer *buf, size_t pos);
i8 bufferGetI8(Buffer *buf, size_t pos);
i16 bufferGetI16(Buffer *buf, size_t pos);
i32 bufferGetI32(Buffer *buf, size_t pos);
f32 bufferGetF32(Buffer *buf, size_t pos);
f64 bufferGetF64(Buffer *buf, size_t pos);
void bufferSetU8(Buffer *buf, size_t pos, u8 value);
void bufferSetU16(Buffer *buf, size_t pos, u16 value);
void bufferSetU32(Buffer *buf, size_t pos, u32 value);
void bufferSetI8(Buffer *buf, size_t pos, i8 value);
void bufferSetI16(Buffer *buf, size_t pos, i16 value);
void bufferSetI32(Buffer *buf, size_t pos, i32 value);
void bufferSetF32(Buffer *buf, size_t pos, f32 value);
void bufferSetF64(Buffer *buf, size_t pos, f64 value);
void bufferSetBytes(Buffer *buf, size_t pos, void *data, size_t length);
void bufferAddU8(Buffer *buf, u8 value);
void bufferAddU16(Buffer *buf, u16 value);
void bufferAddU32(Buffer *buf, u32 value);
void bufferAddI8(Buffer *buf, i8 value);
void bufferAddI16(Buffer *buf, i16 value);
void bufferAddI32(Buffer *buf, i32 value);
void bufferAddF32(Buffer *buf, f32 value);
void bufferAddF64(Buffer *buf, f64 value);
void bufferAddBytes(Buffer *buf, const void *data, size_t length);

String *bufferToString(Buffer *buf);

void bputnumber(Buffer *buf, double number);
void bputchar(Buffer *buf, char ch);
void bputstrlen(Buffer *buf, const char *chars, size_t byteLength);
void bputstr(Buffer *buf, const char *string);
void bprintf(Buffer *buf, const char *format, ...) MTOTS_PRINTFLIKE(2, 3);

#endif /*mtots_util_buffer_h*/

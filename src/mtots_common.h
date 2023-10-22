#ifndef mtots_common_h
#define mtots_common_h

#include <limits.h>
#include <stddef.h>

#include "mtots_config.h"

/* A macro that is `#define`d to nothing */
#define MTOTS_NOTHING

#define U8_MAX 0xFF
#define U16_MAX 65535
#define U32_MAX 4294967295U
#define U64_MAX 0xFFFFFFFFFFFFFFFF
#define I8_MAX 127
#define I16_MIN (-32768)
#define I16_MAX 32767
#define I32_MIN (-2147483647 - 1) /* When using "-2147483648" Windows assumes unsigned */
#define I32_MAX 2147483647
#define I64_MIN (-0x7FFFFFFFFFFFFFFF - 1)
#define I64_MAX 0x7FFFFFFFFFFFFFFF

#define UTRUE 1
#define UFALSE 0

#define U8_COUNT (U8_MAX + 1)
#define MAX_ARG_COUNT 127

/* PLATFORM DEPENDENT NOTE: Strictly speaking, C89 does not
 * guarantee that char always holds 8 bits (e.g. on some
 * ancient platforms a byte may be 7-bits).
 * Further short is not guaranteed to be exactly 2 bytes.
 *
 * However, on almost every modern platform, a byte is
 * 8 bits, and short is 2 bytes.
 *
 * Assuming int is 32-bits may seem a bit more of a stretch,
 * but actually ILP64 are relatively less common. Most 64-bit
 * platforms seem to prefer LP64 where int stays 32-bits and
 * only long ints and pointers are 64 bits.
 * See https://unix.org/version2/whatsnew/lp64_wp.html
 * "64-Bit Programming Models: Why LP64?"
 *
 * Even Win64 is a LLP64 model which means int is still 32 bits:
 * https://devblogs.microsoft.com/oldnewthing/20050131-00/?p=36563
 */
typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;

#if LONG_MAX == 0x7FFFFFFFFFFFFFFF
typedef signed long i64;
typedef unsigned long u64;
#elif defined(LLONG_MAX) && LLONG_MAX == 0x7FFFFFFFFFFFFFFF
typedef signed long long i64;
typedef unsigned long long u64;
#else
#error "neither long nor long long are 64-bits"
#endif

typedef u8 ubool;

typedef float f32;
typedef double f64;

typedef enum Status {
  STATUS_ERROR,
  STATUS_OK
} Status;

#endif /*mtots_common_h*/

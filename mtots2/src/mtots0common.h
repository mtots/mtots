#ifndef mtots0common_h
#define mtots0common_h

#include <limits.h>
#include <stddef.h>

#include "mtots0config.h"

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

typedef unsigned char u8;
typedef signed char i8;

#if SHRT_MAX == 0x7FFF
typedef unsigned short u16;
typedef signed short i16;
#else
#error "short is not 16-bits"
#endif

#if INT_MAX == 0x7FFFFFFF
typedef unsigned int u32;
typedef signed int i32;
#else
#error "int is not 32-bits"
#endif

#if LONG_MAX == 0x7FFFFFFFFFFFFFFF
typedef signed long i64;
typedef unsigned long u64;
#elif LLONG_MAX == 0x7FFFFFFFFFFFFFFF
typedef signed long long i64;
typedef unsigned long long u64;
#else
#error "neither long nor long long are 64-bits"
#endif

typedef u8 ubool;

typedef float f32;
typedef double f64;

typedef enum NODISCARD Status {
  STATUS_ERR,
  STATUS_OK
} Status;

#endif /*mtots0common_h*/

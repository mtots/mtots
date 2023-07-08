#ifndef mtots_assumptions_h
#define mtots_assumptions_h

#include <limits.h>

/* 8-bits in a byte */
#if CHAR_BIT != 8
#error "CHAR_BIT != 8"
#endif
#if SCHAR_MIN != -128 || SCHAR_MAX != 127
#error "SCHAR_MIN != -128 || SCHAR_MAX != 127"
#endif
#if UCHAR_MAX != 255
#error "UCHAR_MAX != 255"
#endif

/* short should be 16-bits */
#if SHRT_MIN != -32768 || SHRT_MAX != 32767
#error "SHRT_MIN != -32767 || SHRT_MAX != 32767"
#endif
#if USHRT_MAX != 65535
#error "USHRT_MAX != 65535"
#endif

/* int should be 32-bits */
#if INT_MIN != -2147483648 || INT_MAX != 2147483647
#error "INT_MIN != -2147483648 || INT_MAX != 2147483647"
#endif
#if UINT_MAX != 4294967295
#error "UINT_MAX != 4294967295"
#endif

/* Some assumptions cannot be checked just with macros
 * (e.g. endianness)*/
void checkAssumptions(void);

#endif/*mtots_assumptions_h*/

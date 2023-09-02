#ifndef mtots2structs_h
#define mtots2structs_h

#include "mtots2list.h"
#include "mtots2object.h"
#include "mtots2string.h"

/* Internal struct definitions for various objects.
 * They need to be included in exactly two places:
 * - the C file where their operations are defined, and
 * - the object.c file where they are freed */

struct String {
  Object object;
  size_t byteCapacity;   /* Number of bytes allocated */
  size_t byteLength;     /* Number of bytes in the UTF-8 encoding (excluding null) */
  size_t codePointCount; /* Number of unicode code points in this String */
  u8 *utf8;              /* UTF-8 encoding of the string */
  u32 *utf32;            /* NULL if string is ASCII otherwise the UTF-32 encoding */
  u32 hash;
  ubool frozen;
};

struct List {
  Object object;
  size_t length;
  size_t capacity;
  Value *buffer;
  u32 hash;
  ubool frozen;
};

#endif /*mtots2structs_h*/

#ifndef mtots_util_string_h
#define mtots_util_string_h

#include "mtots_common.h"

typedef struct String {
  ubool isMarked;
  ubool isForever;       /* if true, this string should never be freed */
  char *chars;           /* UTF-8 encoding of the string */
  u32 *utf32;            /* UTF-32 encoding, if this string is not ASCII */
  size_t byteLength;     /* Number of bytes in the UTF-8 encoding */
  size_t codePointCount; /* Number of unicode code points in this String */
  u32 hash;
} String;

typedef struct CommonStrings {
  String *empty;
  String *init;
  String *iter;
  String *len;
  String *repr;
  String *add;
  String *sub;
  String *mul;
  String *div;
  String *floordiv;
  String *mod;
  String *pow;
  String *neg;
  String *contains;
  String *nil;
  String * true;
  String * false;
  String *getitem;
  String *setitem;
  String *slice;
  String *getattr;
  String *setattr;
  String *call;
  String *red;
  String *green;
  String *blue;
  String *alpha;
  String *r;
  String *g;
  String *b;
  String *a;
  String *w;
  String *h;
  String *x;
  String *y;
  String *z;
  String *type;
  String *width;
  String *height;
  String *minX;
  String *minY;
  String *maxX;
  String *maxY;
  String *oneCharAsciiStrings[128];
} CommonStrings;

String *internString(const char *chars, size_t length);
String *internCString(const char *string);
String *internOwnedString(char *chars, size_t length);
String *internForeverCString(const char *string);
String *internForeverString(const char *chars, size_t len);
Status internUTF32(const u32 *utf32, size_t codePointCount, String **out);
Status sliceString(String *string, size_t start, size_t end, String **out);
size_t getInternedStringsAllocationSize(void);
void freeUnmarkedStrings(void);
CommonStrings *getCommonStrings(void);

#endif /*mtots_util_string_h*/

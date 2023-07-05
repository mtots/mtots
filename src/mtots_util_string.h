#ifndef mtots_util_string_h
#define mtots_util_string_h

#include "mtots_common.h"

typedef struct String {
  ubool isMarked;
  char *chars;            /* UTF-8 encoding of the string */
  u32 *utf32;             /* UTF-32 encoding, if this string is not ASCII */
  size_t byteLength;      /* Number of bytes in the UTF-8 encoding */
  size_t codePointCount;  /* Number of unicode code points in this String */
  u32 hash;
} String;

void initSpecialStrings();
String *internString(const char *chars, size_t length);
String *internCString(const char *string);
String *internOwnedString(char *chars, size_t length);
ubool internUTF32(const u32 *utf32, size_t codePointCount, String **out);
ubool sliceString(String *string, size_t start, size_t end, String **out);
size_t getInternedStringsAllocationSize();
void freeUnmarkedStrings();


#endif/*mtots_util_string_h*/

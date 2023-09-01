#ifndef mtots2string_h
#define mtots2string_h

#include "mtots2object.h"

void retainString(String *string);
void releaseString(String *string);
Value stringValue(String *string);
ubool isString(Value value);
String *asString(Value value);

void reprString(String *out, String *string);
ubool eqString(String *a, String *b);
u32 hashString(String *a);
void freezeString(String *a);
size_t lenString(String *a);

NODISCARD String *newStringWithLength(const char *chars, size_t len);
NODISCARD String *newString(const char *cstring);

const char *stringChars(String *a);
size_t stringByteLength(String *a);
u32 stringCharAt(String *a, size_t index);

/* Like printf, but appends to the given string
 * 'mtots string printf' */
void msprintf(String *out, const char *fmt, ...) MTOTS_PRINTFLIKE(2, 3);

/* Like fputc, but appends to the given string
 * 'mtots string putc' */
void msputc(char ch, String *out);

/* Like fputs, but appends to the given string
 * 'mtots string puts' */
void msputs(const char *cstr, String *out);

#endif /*mtots2string_h*/

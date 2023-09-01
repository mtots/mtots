#include "mtots2string.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots1err.h"
#include "mtots1unicode.h"
#include "mtots2structs.h"

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

void retainString(String *string) {
  retainObject((Object *)string);
}

void releaseString(String *string) {
  releaseObject((Object *)string);
}

Value stringValue(String *string) {
  return objectValue((Object *)string);
}

ubool isString(Value value) {
  return value.type == VALUE_OBJECT &&
         value.as.object->type == OBJECT_STRING;
}

String *asString(Value value) {
  if (!isString(value)) {
    panic("Expected String but got %s", getValueKindName(value));
  }
  return (String *)value.as.object;
}

void reprString(String *out, String *string) {
  size_t i;
  msputc('"', out);
  for (i = 0; i < string->byteLength; i++) {
    switch (string->utf8[i]) {
      case '\"':
        msputs("\\\"", out);
        break;
      case '\t':
        msputs("\\t", out);
        break;
      case '\r':
        msputs("\\r", out);
        break;
      default:
        msputc((char)string->utf8[i], out);
    }
  }
  msputc('"', out);
}

ubool eqString(String *a, String *b) {
  if (a == b) {
    return UTRUE;
  }
  if (a->byteLength != b->byteLength) {
    return UFALSE;
  }
  if (a->frozen && b->frozen && a->hash != b->hash) {
    return UFALSE;
  }
  return memcmp(a->utf8, b->utf8, a->byteLength) == 0;
}

u32 hashString(String *a) {
  freezeString(a);
  return a->hash;
}

/** Returned result must be freed */
NODISCARD static u32 *utf8ToUtf32(const u8 *chars, size_t byteLength, size_t *codePointCount) {
  size_t i, count = 0;
  const u8 *end = chars + byteLength, *p = chars;
  u32 *utf32;
  while (*p) {
    int charSize = decodeUTF8Char(p, end, NULL);
    if (charSize < 1) {
      panic("Tried to convert invalid UTF8 to str");
    }
    p += charSize;
    count++;
  }
  utf32 = (u32 *)malloc(sizeof(u32) * count);
  p = chars;
  i = 0;
  while (*p) {
    p += decodeUTF8Char(p, end, utf32 + (i++));
  }
  if (codePointCount) {
    *codePointCount = count;
  }
  return utf32;
}

void freezeString(String *a) {
  size_t i;
  ubool ascii = UTRUE;
  if (a->frozen) {
    return;
  }
  a->frozen = UTRUE;
  a->byteCapacity = a->byteLength + 1;
  a->utf8 = (u8 *)realloc(a->utf8, a->byteCapacity);
  a->hash = hashStringData(a->utf8, a->byteLength);
  for (i = 0; i < a->byteLength; i++) {
    if (a->utf8[i] >= 128) {
      ascii = UFALSE;
      break;
    }
  }
  if (ascii) {
    a->codePointCount = a->byteLength;
  } else {
    a->utf32 = utf8ToUtf32(a->utf8, a->byteLength, &a->codePointCount);
  }
}

size_t lenString(String *a) {
  freezeString(a);
  return a->codePointCount;
}

NODISCARD String *newStringWithLength(const char *chars, size_t len) {
  String *str = (String *)calloc(1, sizeof(String));
  str->object.type = OBJECT_STRING;
  str->byteCapacity = 1 + (str->byteLength = len);
  str->utf8 = (u8 *)malloc(len + 1);
  memcpy(str->utf8, chars, len);
  str->utf8[len] = '\0';
  return str;
}

NODISCARD String *newString(const char *cstring) {
  return newStringWithLength(cstring, strlen(cstring));
}

const char *stringChars(String *a) {
  return (const char *)a->utf8;
}

size_t stringByteLength(String *a) {
  return a->byteLength;
}

u32 stringCharAt(String *a, size_t index) {
  freezeString(a);
  return a->utf32 ? a->utf32[index] : (u32)a->utf8[index];
}

void msprintf(String *out, const char *format, ...) {
  va_list args;
  int size;
  size_t mincap;

  if (out->frozen) {
    panic("Tried to append to a frozen String");
  }

  va_start(args, format);
  size = vsnprintf(NULL, 0, format, args);
  va_end(args);

  if (size < 0) {
    panic("sbprintf: encoding error");
  }

  mincap = out->byteLength + ((size_t)size) + 1;
  while (out->byteCapacity < mincap) {
    out->byteCapacity = GROW_CAPACITY(out->byteCapacity);
  }
  out->utf8 = (u8 *)realloc(out->utf8, out->byteCapacity);

  va_start(args, format);
  vsnprintf((char *)(out->utf8 + out->byteLength), out->byteCapacity, format, args);
  va_end(args);
  out->byteLength += ((size_t)size);
}

void msputc(char ch, String *out) {
  /* TODO: implement more efficiently */
  msprintf(out, "%c", ch);
}

void msputs(const char *cstr, String *out) {
  /* TODO: implement more efficiently */
  msprintf(out, "%s", cstr);
}

#include "mtots2string.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots1err.h"
#include "mtots1unicode.h"

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

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

static void freeString(Object *object) {
  String *string = (String *)object;
  free(string->utf8);
  free(string->utf32);
}

Class STRING_CLASS = {
    "String",   /* name */
    0,          /* size */
    NULL,       /* constructor */
    freeString, /* desctructor */
};

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
  str->byteLength = len;
  str->byteCapacity = str->byteLength + 1;
  str->utf8 = (u8 *)malloc(str->byteCapacity);
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

static void adjustCapacity(String *str, size_t mincap) {
  if (str->byteCapacity < mincap) {
    while (str->byteCapacity < mincap) {
      str->byteCapacity = GROW_CAPACITY(str->byteCapacity);
    }
    str->utf8 = (u8 *)realloc(str->utf8, str->byteCapacity);
  }
}

NODISCARD String *addStrings(String *a, String *b) {
  String *ret = newStringWithLength((const char *)a->utf8, a->byteLength);
  stringAppend(ret, (const char *)b->utf8, b->byteLength);
  return ret;
}

void stringAppend(String *out, const char *chars, size_t length) {
  if (out->frozen) {
    panic("Cannot append to a frozen String");
  }
  adjustCapacity(out, out->byteLength + length + 1);
  memcpy(out->utf8 + out->byteLength, chars, length);
  out->byteLength += length;
  out->utf8[out->byteLength] = '\0';
}

void msprintf(String *out, const char *format, ...) {
  va_list args;
  int size;

  if (out->frozen) {
    panic("Cannot append to a frozen String");
  }

  va_start(args, format);
  size = vsnprintf(NULL, 0, format, args);
  va_end(args);

  if (size < 0) {
    panic("sbprintf: encoding error");
  }

  adjustCapacity(out, out->byteLength + ((size_t)size) + 1);

  va_start(args, format);
  vsnprintf((char *)(out->utf8 + out->byteLength), out->byteCapacity, format, args);
  va_end(args);
  out->byteLength += ((size_t)size);
}

void msputc(char ch, String *out) {
  stringAppend(out, &ch, 1);
}

void msputs(const char *cstr, String *out) {
  stringAppend(out, cstr, strlen(cstr));
}

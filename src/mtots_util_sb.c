#include "mtots_util_sb.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_util_error.h"

static void setSBLength(StringBuilder *sb, size_t newLength) {
  if (sb->capacity < newLength + 1) {
    while (sb->capacity < newLength + 1) {
      sb->capacity = sb->capacity < 8 ? 8 : sb->capacity * 2;
    }
    sb->buffer = (char *)realloc(sb->buffer, sb->capacity);
  }
  sb->length = newLength;
  sb->buffer[newLength] = '\0';
}

void initStringBuilder(StringBuilder *sb) {
  sb->buffer = NULL;
  sb->capacity = sb->length = 0;
}

void freeStringBuilder(StringBuilder *sb) {
  free(sb->buffer);
}

void sbclear(StringBuilder *sb) {
  if (sb->buffer) {
    sb->length = 0;
    sb->buffer[0] = '\0';
  }
}

String *sbstring(StringBuilder *sb) {
  return internString(sb->buffer, sb->length);
}

void sbputnumber(StringBuilder *sb, double number) {
  if (number != number) {
    sbprintf(sb, "nan");
    return;
  }
  /* Trim the trailing zeros in the number representation */
  sbprintf(sb, "%f", number);
  while (sb->length > 0 && sb->buffer[sb->length - 1] == '0') {
    sb->buffer[--sb->length] = '\0';
  }
  if (sb->length > 0 && sb->buffer[sb->length - 1] == '.') {
    sb->buffer[--sb->length] = '\0';
  }
}

void sbputchar(StringBuilder *sb, char ch) {
  setSBLength(sb, sb->length + 1);
  sb->buffer[sb->length - 1] = ch;
}

void sbputstrlen(StringBuilder *sb, const char *chars, size_t byteLength) {
  size_t startLength = sb->length;
  setSBLength(sb, startLength + byteLength);
  memcpy(sb->buffer + startLength, chars, byteLength);
}

void sbputstr(StringBuilder *sb, const char *string) {
  sbputstrlen(sb, string, strlen(string));
}

void sbprintf(StringBuilder *sb, const char *format, ...) {
  va_list args;
  int intSize;
  size_t size, startLength = sb->length;

  va_start(args, format);
  intSize = vsnprintf(NULL, 0, format, args);
  va_end(args);

  if (intSize < 0) {
    panic("sbprintf: encoding error");
  }

  size = (size_t)intSize;

  setSBLength(sb, startLength + size);

  va_start(args, format);
  vsnprintf(sb->buffer + startLength, size + 1, format, args);
  va_end(args);
}

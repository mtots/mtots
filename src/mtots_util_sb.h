#ifndef mtots_util_sb_h
#define mtots_util_sb_h

#include "mtots_common.h"

typedef struct StringBuilder {
  char *buffer;
  size_t length;
  size_t capacity;
} StringBuilder;

void initStringBuilder(StringBuilder *sb);
void sbputnumber(StringBuilder *sb, double number);
void sbputchar(StringBuilder *sb, char ch);
void sbputstrlen(StringBuilder *sb, const char *chars, size_t byteLength);
void sbputstr(StringBuilder *sb, const char *string);
void sbprintf(StringBuilder *sb, const char *format, ...) MTOTS_PRINTFLIKE(2, 3);

#endif /*mtots_util_sb_h*/

#ifndef mtots_m_json_parse_h
#define mtots_m_json_parse_h

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

/* Quick and dirty JSON parser based on the state machine
 * diagram here: https://www.json.org/json-en.html
 */
typedef struct JSONParseState {
  const char *start, *ptr;
  size_t line, col;
} JSONParseState;

static ubool parseOneBlob(JSONParseState *s);

static void initJSONParseState(JSONParseState *s, const char *str) {
  s->start = s->ptr = str;
  s->line = s->col = 1;
}

static char peek(JSONParseState *s) {
  return s->ptr[0];
}

static void incr(JSONParseState *s) {
  char c = peek(s);
  if (c != '\0') {
    if (c == '\n') {
      s->line++;
      s->col = 1;
    } else {
      s->col++;
    }
    s->ptr++;
  }
}

static ubool isWhitespace(char c) {
  switch (c) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
    case '\f':
    case '\v':
      return STATUS_OK;
  }
  return STATUS_ERROR;
}

static void skipWhitespace(JSONParseState *s) {
  while (isWhitespace(peek(s))) {
    incr(s);
  }
}

static ubool startsWith(JSONParseState *s, const char *prefix) {
  return strncmp(s->ptr, prefix, strlen(prefix)) == 0;
}

static int interpHexDigit(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
  if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
  return -1;
}

static ubool scanString(JSONParseState *s, size_t *outNBytes, char *out) {
  size_t nbytes = 0;
  char *p = out;

  while (peek(s) != '"' && peek(s) != '\0') {
    if (peek(s) == '\\') {
      /* escape */
      incr(s);
      switch (peek(s)) {
        case '"':
          nbytes++;
          if (p) *p++ = '"';
          incr(s);
          break;
        case '\\':
          nbytes++;
          if (p) *p++ = '\\';
          incr(s);
          break;
        case '/':
          nbytes++;
          if (p) *p++ = '/';
          incr(s);
          break;
        case 'b':
          nbytes++;
          if (p) *p++ = '\b';
          incr(s);
          break;
        case 'f':
          nbytes++;
          if (p) *p++ = '\f';
          incr(s);
          break;
        case 'n':
          nbytes++;
          if (p) *p++ = '\n';
          incr(s);
          break;
        case 'r':
          nbytes++;
          if (p) *p++ = '\r';
          incr(s);
          break;
        case 't':
          nbytes++;
          if (p) *p++ = '\t';
          incr(s);
          break;
        case 'u': {
          int charBytes = 0;
          u32 codePoint = 0;
          incr(s); /* skip 'u' */
          if (interpHexDigit(s->ptr[0]) == -1 ||
              interpHexDigit(s->ptr[1]) == -1 ||
              interpHexDigit(s->ptr[2]) == -1 ||
              interpHexDigit(s->ptr[3]) == -1) {
            runtimeError(
                "while parsing JSON, invalid hex digit "
                "on line %lu column %lu",
                (unsigned long)s->line, (unsigned long)s->col);
            return STATUS_ERROR;
          }
          codePoint =
              (u32)interpHexDigit(s->ptr[0]) << 24 |
              (u32)interpHexDigit(s->ptr[1]) << 16 |
              (u32)interpHexDigit(s->ptr[2]) << 8 |
              (u32)interpHexDigit(s->ptr[3]);
          charBytes = encodeUTF8Char(codePoint, p);
          if (p) p += charBytes;
          nbytes += charBytes;
          incr(s); /* hex digit 1 */
          incr(s); /* hex digit 2 */
          incr(s); /* hex digit 3 */
          incr(s); /* hex digit 4 */
          break;
        }
        default:
          runtimeError(
              "while parsing JSON, invalid string escape '%c' (%d) "
              "on line %lu column %lu",
              peek(s), (int)(peek(s)),
              (unsigned long)s->line, (unsigned long)s->col);
          return STATUS_ERROR;
      }
    } else {
      /* Strictly speaking, if the string contains invalid utf8
       * sequences, this is an invalid string. But we'll
       * later check it when we convert to an mtots string */
      nbytes++;
      if (p) {
        *p++ = peek(s);
      }
      incr(s);
    }
  }

  if (outNBytes) {
    *outNBytes = nbytes;
  }
  return STATUS_OK;
}

static ubool parseString(JSONParseState *s) {
  JSONParseState savedState;
  size_t len;
  char *chars;

  if (peek(s) != '"') {
    runtimeError(
        "while parsing JSON, expected '\"' on line %lu column %lu",
        (unsigned long)s->line, (unsigned long)s->col);
    return STATUS_ERROR;
  }
  incr(s); /* starting '"' */

  savedState = *s;
  if (!scanString(s, &len, NULL)) {
    return STATUS_ERROR;
  }
  chars = malloc(sizeof(char) * (len + 1));

  /* rewind now, and fill the buffer this time */
  *s = savedState;
  if (!scanString(s, NULL, chars)) {
    /* This is a more critical failure, since we expected
     * the second scan to be successful */
    panic("assertion error in JSON parseString");
  }
  chars[len] = '\0';

  if (peek(s) != '"') {
    runtimeError(
        "while parsing JSON, missing matching quote for quote on line %lu column %lu",
        (unsigned long)savedState.line, (unsigned long)(savedState.col - 1));
    return STATUS_ERROR;
  }

  incr(s); /* ending '"' */

  push(STRING_VAL(internOwnedString(chars, len)));
  return STATUS_OK;
}

static ubool parseObject(JSONParseState *s) {
  size_t count = 0, i;
  ObjDict *dict;
  if (peek(s) != '{') {
    runtimeError(
        "while parsing JSON, expected '{' on line %lu column %lu",
        (unsigned long)s->line, (unsigned long)s->col);
    return STATUS_ERROR;
  }
  incr(s); /* '{' */
  skipWhitespace(s);
  while (peek(s) != '}' && peek(s) != '\0') {
    count++;
    parseString(s);
    skipWhitespace(s);
    if (peek(s) != ':') {
      runtimeError(
          "while parsing JSON, expected ':' on line %lu column %lu",
          (unsigned long)s->line, (unsigned long)s->col);
      return STATUS_ERROR;
    }
    incr(s); /* ':' */
    skipWhitespace(s);
    parseOneBlob(s);
    skipWhitespace(s);
    if (peek(s) != ',') {
      break;
    }
    incr(s); /* ',' */
    skipWhitespace(s);
  }
  if (peek(s) != '}') {
    runtimeError(
        "while parsing JSON, expected '}' but got '%c' on line %lu column %lu",
        peek(s), (unsigned long)s->line, (unsigned long)s->col);
    return STATUS_ERROR;
  }
  incr(s); /* '}' */
  dict = newDict();
  push(DICT_VAL(dict));
  for (i = 0; i < count; i++) {
    Value key = vm.stackTop[-(2 * (i32)count) - 1 + 2 * i];
    Value value = vm.stackTop[-(2 * (i32)count) - 1 + 2 * i + 1];
    mapSet(&dict->map, key, value);
  }
  pop(); /* dict */
  vm.stackTop -= 2 * count;
  push(DICT_VAL(dict));
  return STATUS_OK;
}

static ubool parseArray(JSONParseState *s) {
  size_t count = 0, i;
  ObjList *list;
  if (peek(s) != '[') {
    runtimeError(
        "while parsing JSON, expected '[' on line %lu column %lu",
        (unsigned long)s->line, (unsigned long)s->col);
    return STATUS_ERROR;
  }
  incr(s); /* '{' */
  skipWhitespace(s);
  while (peek(s) != ']' && peek(s) != '\0') {
    count++;
    parseOneBlob(s);
    skipWhitespace(s);
    if (peek(s) != ',') {
      break;
    }
    incr(s); /* ',' */
    skipWhitespace(s);
  }
  if (peek(s) != ']') {
    runtimeError(
        "while parsing JSON, expected ']' on line %lu column %lu",
        (unsigned long)s->line, (unsigned long)s->col);
    return STATUS_ERROR;
  }
  incr(s); /* ']' */
  list = newList(count);
  for (i = 0; i < count; i++) {
    list->buffer[i] = vm.stackTop[-(i32)count + i];
  }
  vm.stackTop -= count;
  push(LIST_VAL(list));
  return STATUS_OK;
}

static ubool isDigit(char c) {
  return c >= '0' && c <= '9';
}

static ubool parseNumber(JSONParseState *s) {
  const char *start = s->ptr;
  if (peek(s) == '-') {
    incr(s);
  }
  if (!isDigit(peek(s))) {
    runtimeError(
        "while parsing JSON, expected digit on line %lu column %lu",
        (unsigned long)s->line, (unsigned long)s->col);
    return STATUS_ERROR;
  }
  while (isDigit(peek(s))) {
    incr(s);
  }
  if (peek(s) == '.') {
    incr(s);
    while (isDigit(peek(s))) {
      incr(s);
    }
  }
  if (peek(s) == 'e' || peek(s) == 'E') {
    incr(s);
    if (peek(s) == '+' || peek(s) == '-') {
      incr(s);
    }
    while (isDigit(peek(s))) {
      incr(s);
    }
  }
  push(NUMBER_VAL(atof(start)));
  return STATUS_OK;
}

static ubool parseOneBlob(JSONParseState *s) {
  char c;
  skipWhitespace(s);
  c = peek(s);
  if (c == '-' || (c >= '0' && c <= '9')) {
    return parseNumber(s);
  }
  switch (c) {
    case '{':
      return parseObject(s);
    case '[':
      return parseArray(s);
    case '"':
      return parseString(s);
    case 'f':
      if (startsWith(s, "false")) {
        size_t i, len = strlen("false");
        for (i = 0; i < len; i++) {
          incr(s);
        }
        push(BOOL_VAL(UFALSE));
        return STATUS_OK;
      }
      break;
    case 'n':
      if (startsWith(s, "null")) {
        size_t i, len = strlen("null");
        for (i = 0; i < len; i++) {
          incr(s);
        }
        push(NIL_VAL());
        return STATUS_OK;
      }
      break;
    case 't':
      if (startsWith(s, "true")) {
        size_t i, len = strlen("true");
        for (i = 0; i < len; i++) {
          incr(s);
        }
        push(BOOL_VAL(UTRUE));
        return STATUS_OK;
      }
      break;
  }
  runtimeError(
      "while parsing JSON, unrecognized char '%c' (%d) on line %lu column %lu",
      c, (int)c, (unsigned long)s->line, (unsigned long)s->col);
  return STATUS_ERROR;
}

static ubool parseJSON(JSONParseState *s) {
  if (!parseOneBlob(s)) {
    return STATUS_ERROR;
  }
  skipWhitespace(s);
  if (peek(s) != '\0') {
    char c = peek(s);
    runtimeError(
        "while parsing JSON, extra data '%c' (%d) on line %lu column %lu",
        c, (int)c, (unsigned long)s->line, (unsigned long)s->col);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

#endif /*mtots_m_json_parse_h*/

#include "mtots3escape.h"

#include <string.h>

#include "mtots1err.h"
#include "mtots1unicode.h"

#define INVALID_HEX 24

static u32 evalHex(char ch) {
  if (ch >= '0' && ch <= '9') return (u32)(ch - '0');
  if (ch >= 'A' && ch <= 'F') return (u32)(10 + ch - 'A');
  if (ch >= 'a' && ch <= 'f') return (u32)(10 + ch - 'a');
  return INVALID_HEX;
}

Status unescapeString(
    String *out,
    const char *str,
    const char *quote,
    size_t quoteLen) {
  while (*str != '\0' && strncmp(str, quote, quoteLen) != 0) {
    if (*str == '\\') {
      str++;
      switch (*str) {
        case '"':
          str++;
          msputc('\"', out);
          break;
        case '\'':
          str++;
          msputc('\'', out);
          break;
        case '\\':
          str++;
          msputc('\\', out);
          break;
        case '/':
          str++;
          msputc('/', out);
          break;
        case 'b':
          str++;
          msputc('\b', out);
          break;
        case 'f':
          str++;
          msputc('\f', out);
          break;
        case 'n':
          str++;
          msputc('\n', out);
          break;
        case 'r':
          str++;
          msputc('\r', out);
          break;
        case 't':
          str++;
          msputc('\t', out);
          break;
        case '0':
          str++;
          msputc('\0', out);
          break;
        case 'x': {
          u32 byte;
          str++;
          if (evalHex(str[0]) == INVALID_HEX ||
              evalHex(str[1]) == INVALID_HEX) {
            char invalid = evalHex(str[0]) == INVALID_HEX ? str[0] : str[1];
            runtimeError("in string unescape, invalid hex digit '%c'", invalid);
            return STATUS_ERR;
          }
          byte = evalHex(str[0]) << 4 | evalHex(str[1]);
          msputc((char)(unsigned char)byte, out);
          str += 2;
          break;
        }
        case 'u': {
          int charBytes;
          u32 codePoint;
          char charStr[5];
          charStr[0] = charStr[1] = charStr[2] = charStr[3] = charStr[4] = 0;
          str++;
          if (evalHex(str[0]) == INVALID_HEX ||
              evalHex(str[1]) == INVALID_HEX ||
              evalHex(str[2]) == INVALID_HEX ||
              evalHex(str[3]) == INVALID_HEX) {
            char invalid =
                evalHex(str[0]) == INVALID_HEX ? str[0] : evalHex(str[1]) == INVALID_HEX ? str[1]
                                                      : evalHex(str[2]) == INVALID_HEX   ? str[2]
                                                                                         : str[3];
            runtimeError("in string unescape, invalid hex digit '%c'", invalid);
            return STATUS_ERR;
          }
          codePoint =
              evalHex(str[0]) << 12 |
              evalHex(str[1]) << 8 |
              evalHex(str[2]) << 4 |
              evalHex(str[3]);
          charBytes = encodeUTF8Char(codePoint, (u8 *)(char *)charStr);
          stringAppend(out, charStr, (unsigned int)charBytes);
          str += 4;
          break;
        }
        default:
          runtimeError(
              "in string unescape, invalid escape '%c' (%d)",
              *str, (int)*str);
          return STATUS_ERR;
      }
    } else {
      msputc(*str++, out);
    }
  }
  return STATUS_OK;
}

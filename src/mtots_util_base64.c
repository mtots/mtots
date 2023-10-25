#include "mtots_util_base64.h"

#include "mtots_util_error.h"

#define INVALID_CHAR 65

static const char base64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const u8 base64Rmap[] = {
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    62,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    63,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    0,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
    INVALID_CHAR,
};

Status encodeBase64(const u8 *input, size_t length, StringBuilder *out) {
  size_t i;
  size_t lenRem = length % 3;
  size_t roundLen = length - lenRem;
  for (i = 0; i < roundLen; i += 3) {
    u32 chunk =
        (((u32)input[i]) << 16) |
        (((u32)input[i + 1]) << 8) |
        (((u32)input[i + 2]));
    sbputchar(out, base64Alphabet[chunk >> 18]);
    sbputchar(out, base64Alphabet[(chunk >> 12) & 63]);
    sbputchar(out, base64Alphabet[(chunk >> 6) & 63]);
    sbputchar(out, base64Alphabet[chunk & 63]);
  }
  if (lenRem == 1) {
    u32 chunk = ((u32)input[i]) << 16;
    sbputchar(out, base64Alphabet[chunk >> 18]);
    sbputchar(out, base64Alphabet[(chunk >> 12) & 63]);
    sbputchar(out, '=');
    sbputchar(out, '=');
  } else if (lenRem == 2) {
    u32 chunk =
        (((u32)input[i]) << 16) |
        (((u32)input[i + 1]) << 8);
    sbputchar(out, base64Alphabet[chunk >> 18]);
    sbputchar(out, base64Alphabet[(chunk >> 12) & 63]);
    sbputchar(out, base64Alphabet[(chunk >> 6) & 63]);
    sbputchar(out, '=');
  }
  return STATUS_OK;
}

static Status decodeBase64Char(char ch, u32 *out) {
  if (ch >= 43 && ch <= 122) {
    u32 value = (u32)base64Rmap[(u8)ch];
    if (value != INVALID_CHAR) {
      *out = value;
      return STATUS_OK;
    }
  }
  runtimeError("decodeBase64Char: Invalid char %c (%u)", ch, ch);
  return STATUS_ERROR;
}

/* Decode a 4 char chunk from the input */
static Status decodeBase64Chunk(const char *input, Buffer *out) {
  u32 chunk = 0, charVal;

  /* Validation */
  if (input[0] == '=' || input[1] == '=' || (input[2] == '=' && input[3] != '=')) {
    runtimeError("Invalid placement of '='");
    return STATUS_ERROR;
  }

  /* compute chunk */
  if (!decodeBase64Char(input[0], &charVal)) {
    return STATUS_ERROR;
  }
  chunk |= charVal << 18;
  if (!decodeBase64Char(input[1], &charVal)) {
    return STATUS_ERROR;
  }
  chunk |= charVal << 12;
  if (!decodeBase64Char(input[2], &charVal)) {
    return STATUS_ERROR;
  }
  chunk |= charVal << 6;
  if (!decodeBase64Char(input[3], &charVal)) {
    return STATUS_ERROR;
  }
  chunk |= charVal;

  /* emit data */
  /* TODO: Consider adding error handling based on invalid values */
  if (input[2] == '=') {
    bufferAddU8(out, chunk >> 16);
  } else if (input[3] == '=') {
    bufferAddU8(out, chunk >> 16);
    bufferAddU8(out, (chunk >> 8) & 255);
  } else {
    bufferAddU8(out, chunk >> 16);
    bufferAddU8(out, (chunk >> 8) & 255);
    bufferAddU8(out, chunk & 255);
  }
  return STATUS_OK;
}

Status decodeBase64(const char *input, size_t length, Buffer *out) {
  size_t i;
  if (length % 4 != 0) {
    runtimeError(
        "decodeBase64 requires input length that is a multiple of 4, "
        "but got length=%lu",
        (unsigned long)length);
    return STATUS_ERROR;
  }
  for (i = 0; i < length; i += 4) {
    if (!decodeBase64Chunk(input + i, out)) {
      return STATUS_ERROR;
    }
  }
  return STATUS_OK;
}

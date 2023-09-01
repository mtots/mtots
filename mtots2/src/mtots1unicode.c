#include "mtots1unicode.h"

int encodeUTF8Char(u32 codePoint, u8 *outBytes) {
  int nbytes = 0;

  /* Codepoint ranges from https://en.wikipedia.org/wiki/UTF-8#Encoding */
  if (codePoint <= 0x7F) {
    nbytes = 1;
  } else if (codePoint <= 0x7FF) {
    nbytes = 2;
  } else if (codePoint <= 0xFFFF) {
    nbytes = 3;
  } else if (codePoint <= 0x10FFFF) {
    nbytes = 4;
  } else {
    return 0; /* not legal codePoint */
  }

  if (outBytes) {
    /* NOTE:
     *   0b10000000 = 0x80 (OR for continuation byte)
     *   0b11000000 = 0xC0 (top 2 bits set)
     *   0b11100000 = 0xE0 (top 3 bits set)
     *   0b11110000 = 0xF0 (top 4 bits set)
     *   0b00111111 = 0x3F (mask for lowest 6 bits)
     */
    switch (nbytes) {
      case 1:
        outBytes[0] = (u8)codePoint;
        break;
      case 2:
        outBytes[0] = (u8)(0xC0 | (codePoint >> 6));
        outBytes[1] = (u8)(0x80 | (codePoint & 0x3F));
        break;
      case 3:
        outBytes[0] = (u8)(0xE0 | (codePoint >> 12));
        outBytes[1] = (u8)(0x80 | ((codePoint >> 6) & 0x3F));
        outBytes[2] = (u8)(0x80 | (codePoint & 0x3F));
        break;
      case 4:
        outBytes[0] = (u8)(0xF0 | (codePoint >> 18));
        outBytes[1] = (u8)(0x80 | ((codePoint >> 12) & 0x3F));
        outBytes[2] = (u8)(0x80 | ((codePoint >> 6) & 0x3F));
        outBytes[3] = (u8)(0x80 | (codePoint & 0x3F));
        break;
    }
  }

  return nbytes;
}

int decodeUTF8Char(const u8 *bytes, const u8 *limit, u32 *outCodePoint) {
  int i, nbytes = 0;
  u32 byte1 = (u32)bytes[0], byte2, byte3, byte4;

  /* NOTE:
   *   0b01111111 = 0x7F (1-byte, lower limit is 0)
   *
   *   0b11000000 = 0xC0 (2-byte)
   *   0b11011111 = 0xDF
   *
   *   0b11100000 = 0xE0 (3-byte)
   *   0b11101111 = 0xEF
   *
   *   0b11110000 = 0xF0 (4-byte)
   *   0b11110111 = 0xF7
   */
  if (byte1 <= 0x7F) {
    nbytes = 1;
  } else if (byte1 >= 0xC0 && byte1 <= 0xDF) {
    nbytes = 2;
  } else if (byte1 >= 0xE0 && byte1 <= 0xEF) {
    nbytes = 3;
  } else if (byte1 >= 0xF0 && byte1 <= 0xF7) {
    nbytes = 4;
  } else {
    return 0; /* Invalid leading byte */
  }

  if (bytes + nbytes > limit) {
    return 0; /* Not enough bytes to be a valid sequence */
  }

  for (i = 1; i < nbytes; i++) {
    /* continuation bytes must all start with bits '10' */
    if (bytes[i] >> 6 != 2) {
      return 0;
    }
  }

  if (outCodePoint) {
    /* NOTE:
     *   0b00111111 = 0x3F (lower 6 bits set)
     *   0b00011111 = 0x1F (lower 5 bits set)
     *   0b00001111 = 0x0F (lower 4 bits set)
     *   0b00000111 = 0x07 (lower 3 bits set)
     */
    switch (nbytes) {
      case 1:
        *outCodePoint = byte1;
        break;
      case 2:
        byte2 = (u32)bytes[1];
        *outCodePoint =
            ((byte1 & 0x1F) << 6) |
            (byte2 & 0x3F);
        break;
      case 3:
        byte2 = (u32)bytes[1];
        byte3 = (u32)bytes[2];
        *outCodePoint =
            ((byte1 & 0x0F) << 12) |
            ((byte2 & 0x3F) << 6) |
            (byte3 & 0x3F);
        break;
      case 4:
        byte2 = (u32)bytes[1];
        byte3 = (u32)bytes[2];
        byte4 = (u32)bytes[3];
        *outCodePoint =
            ((byte1 & 0x07) << 18) |
            ((byte2 & 0x3F) << 12) |
            ((byte3 & 0x3F) << 6) |
            (byte4 & 0x3F);
        break;
    }
  }

  return nbytes;
}

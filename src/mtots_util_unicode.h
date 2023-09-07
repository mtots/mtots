#ifndef mtots_util_unicode_h
#define mtots_util_unicode_h

/* Basic Unicode utilities */

#include "mtots_common.h"

/* Encodes the unicode codePoint to the buffer out as UTF-8.
 * If out is not NULL, writes 1-4 bytes.
 * If out is NULL, nothing is written.
 * In either case, returns the number of bytes (1-4) the given
 * codePoint expands to in UTF-8.
 *
 * if 'codePoint' is not a valid codePoint, returns 0.
 */
int encodeUTF8Char(u32 codePoint, char *outBytes);

/* Decodes the bytes reading 1-4 bytes.
 * Returns the number of bytes read.
 * if outCodePoint is non-NULL, stores the codePoint value there
 *
 * if 'bytes' is not the start of a valid UTF-8 sequence, returns 0.
 */
int decodeUTF8Char(const char *bytes, const char *limit, u32 *outCodePoint);

#endif /*mtots_util_unicode_h*/

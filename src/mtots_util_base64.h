#ifndef mtots_util_base64_h
#define mtots_util_base64_h

#include "mtots_util_buffer.h"
#include "mtots_util_sb.h"

ubool encodeBase64(const u8 *input, size_t length, StringBuilder *out);
ubool decodeBase64(const char *input, size_t length, Buffer *out);

#endif /*mtots_util_base64_h*/

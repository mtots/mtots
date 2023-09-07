#ifndef mtots_util_readfile_h
#define mtots_util_readfile_h

#include "mtots_util_buffer.h"

ubool readFile(const char *path, void **out, size_t *readFileSize);
ubool readFileIntoBuffer(const char *path, Buffer *out);

#endif /*mtots_util_readfile_h*/

#ifndef mtots_util_readfile_h
#define mtots_util_readfile_h

#include "mtots_util_buffer.h"

Status readFile(const char *path, void **out, size_t *readFileSize);
Status readFileIntoBuffer(const char *path, Buffer *out);

#endif /*mtots_util_readfile_h*/

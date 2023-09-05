#ifndef mtots4readfile_h
#define mtots4readfile_h

#include "mtots2string.h"

/** Read a file and returns its content
 * The returned pointer needs to be `free`d by the caller */
char *readFileAsString(const char *path);

#endif /*mtots4readfile_h*/

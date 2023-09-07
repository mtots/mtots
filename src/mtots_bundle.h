#ifndef mtots_bundle_h
#define mtots_bundle_h

#include "mtots_util_buffer.h"

/* Read a file in the bundle specified by `src` and `path`
 * and append its contents to the given `Buffer` */
ubool readBufferFromBundle(const char *src, const char *path, Buffer *out);

/* Read a file in the bundle specified by `src` and `path`
 * and retrieve its contents as a `String` */
ubool readStringFromBundle(const char *src, const char *path, String **out);

#endif /*mtots_bundl_h*/

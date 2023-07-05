#include "mtots_bundle.h"

#include "mtots_env.h"
#include "mtots_util_readfile.h"

#include <string.h>
#include <stdlib.h>

ubool readBufferFromBundle(const char *src, const char *path, Buffer *out) {
  size_t i, srclen = strlen(src), pathlen = strlen(path);
  char *fullPath;
  ubool status;
  for (i = srclen;
        i > 0 && src[i - 1] != '!' && src[i - 1] != '\\';
        i--);

  if (i && src[i - 1] == '!') {
    /* We assume at this point that this is a zip archive.
     * In this case, we only care about the src starting from
     * after the '!' to find the starting directory.
     * Then we get the final path in the archive by combining
     * with the given path. */
    size_t j, fullPathLen;
    for (j = srclen; j > i && src[j - 1] != '/'; j--);
    fullPathLen = (j - i) + pathlen;
    fullPath = (char*)malloc(fullPathLen + 1);
    memcpy(fullPath, src + i, (j - i));
    strcpy(fullPath + (j - i), path);

    status = readBufferFromMtotsArchive(fullPath, out);
    free(fullPath);
    return status;
  }

  /* Otherwise, we assume that the bundle item is just a regular
   * file that lives in a directory relative to the script source */
  for (i = srclen; i > 0 && src[i - 1] != PATH_SEP; i--);

  fullPath = (char*)malloc(i + 1 + pathlen + 1);
  memcpy(fullPath, src, i);
  fullPath[i] = PATH_SEP;
  strcpy(fullPath + i + 1, path);

  if (PATH_SEP != '/') {
    /* On windows, we probably want to adjust the 'path' part so that
     * we use '\\' */
    size_t j;
    for (j = i + 1; j < i + 1 + pathlen; j++) {
      if (fullPath[j] == '/') {
        fullPath[j] = PATH_SEP;
      }
    }
  }

  status = readFileIntoBuffer(fullPath, out);
  free(fullPath);
  return status;
}

ubool readStringFromBundle(const char *src, const char *path, String **out) {
  Buffer buffer;

  initBuffer(&buffer);
  if (!readBufferFromBundle(src, path, &buffer)) {
    freeBuffer(&buffer);
    return UFALSE;
  }

  *out = internString((const char*)buffer.data, buffer.length);
  freeBuffer(&buffer);
  return UTRUE;
}

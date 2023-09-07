#include "mtots_util_readfile.h"

#include <stdio.h>
#include <stdlib.h>

#include "mtots_util_buffer.h"
#include "mtots_util_error.h"

/*
 * Allocate enough space for the entire contents of a file,
 * read the file and save its contents into the buffer.
 *
 * The resulting buffer (i.e. `out`) must be freed by the caller.
 *
 * For convenience, an extra byte will be allocated at the end of
 * the buffer for the null termiantor ('\0')
 */
ubool readFile(const char *path, void **out, size_t *readFileSize) {
  size_t fileSize;
  FILE *file = fopen(path, "rb");
  void *bytes;
  if (!file) {
    runtimeError("Could not open file \"%s\" for reading", path);
    return UFALSE;
  }

  fseek(file, 0L, SEEK_END);
  fileSize = ftell(file);
  rewind(file);

  bytes = malloc(fileSize + 1);
  if (!bytes) {
    runtimeError("Not enough memory to read \"%s\"\n", path);
    return UFALSE;
  }
  if (fread(bytes, 1, fileSize, file) != fileSize) {
    free(bytes);
    runtimeError("Could not read file \"%s\"", path);
    return UFALSE;
  }
  ((char *)bytes)[fileSize] = '\0';

  *out = bytes;
  if (readFileSize) {
    *readFileSize = fileSize;
  }
  return UTRUE;
}

/*
 * Read the contents of the file specified by `path` and append
 * its contents to the given Buffer
 */
ubool readFileIntoBuffer(const char *path, Buffer *out) {
  size_t fileSize, startSize = out->length;
  FILE *file = fopen(path, "rb");
  if (!file) {
    runtimeError("Could not open file \"%s\" for reading", path);
    return UFALSE;
  }
  fseek(file, 0L, SEEK_END);
  fileSize = ftell(file);
  rewind(file);
  bufferSetLength(out, startSize + fileSize);
  if (fread(out->data + startSize, 1, fileSize, file) != fileSize) {
    runtimeError("Could not read file \"%s\"", path);
    return UFALSE;
  }
  return UTRUE;
}

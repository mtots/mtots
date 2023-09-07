#include "mtots_util_writefile.h"

#include <stdio.h>

#include "mtots_util_error.h"

/*
 * Write some data to a file.
 *
 * Existing content will be overwritten.
 */
ubool writeFile(const void *data, size_t length, const char *path) {
  FILE *file = fopen(path, "wb");
  if (!file) {
    runtimeError("Could not open file \"%s\" for writing", path);
    return UFALSE;
  }

  if (fwrite(data, 1, length, file) < length) {
    fclose(file);
    runtimeError("Error while writing to file \"%s\"", path);
    return UFALSE;
  }

  fclose(file);
  return UTRUE;
}

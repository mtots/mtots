#include "mtots4readfile.h"

#include <stdio.h>
#include <stdlib.h>

#include "mtots1err.h"

char *readFileAsString(const char *path) {
  size_t fileSize;
  char *buffer;
  FILE *file = fopen(path, "rb");
  if (!file) {
    runtimeError("Could not open file \"%s\" for reading", path);
    return NULL;
  }
  fseek(file, 0L, SEEK_END);
  fileSize = (size_t)ftell(file);
  rewind(file);
  buffer = (char *)malloc(fileSize + 1);
  if (fread(buffer, 1, fileSize, file) != fileSize) {
    free(buffer);
    runtimeError("Could not read file \"%s\"", path);
    return NULL;
  }
  buffer[fileSize] = '\0';
  return buffer;
}

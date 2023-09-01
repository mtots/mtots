#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "mtots1err.h"

#define ERROR_BUFFER_SIZE 1024

static char errorString[ERROR_BUFFER_SIZE];

NORETURN void panic(const char *format, ...) {
  va_list args;
  va_start(args, format);
  fflush(stdout);
  fputs("PANIC: ", stderr);
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
  va_end(args);
  abort();
}

void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(errorString, ERROR_BUFFER_SIZE, format, args);
  va_end(args);
}

const char *getErrorString(void) {
  return errorString[0] == '\0' ? NULL : errorString;
}

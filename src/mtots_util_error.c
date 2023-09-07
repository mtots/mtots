#include "mtots_util_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

static void (*errorContextProvider)(Buffer *);
static char *errorString;
static char *savedErrorString;

NORETURN void panic(const char *format, ...) {
  va_list args;
  va_start(args, format);
  fflush(stdout);
#ifdef __ANDROID__
  __android_log_vprint(ANDROID_LOG_ERROR, "MTOTS_RT", format, args);
#else
  vfprintf(stderr, format, args);
  fputs("\n", stderr);
#endif
  va_end(args);
  if (errorContextProvider) {
    Buffer buf;
    initBuffer(&buf);
    errorContextProvider(&buf);
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "MTOTS_RT", "%s", bufferToString(&buf)->chars);
#else
    fprintf(stderr, "%.*s", (int)buf.length, (char *)buf.data);
#endif
    freeBuffer(&buf);
  }
  abort();
}

/* Potentially recoverable error. */
void runtimeError(const char *format, ...) {
  size_t len = 0;
  va_list args;
  char *ptr;
  Buffer buf;

  initBuffer(&buf);

  if (errorContextProvider) {
    errorContextProvider(&buf);
  }

  va_start(args, format);
  len += vsnprintf(NULL, 0, format, args);
  va_end(args);
  len++; /* '\n' */

  len += buf.length;

  ptr = errorString = (char *)realloc(errorString, sizeof(char) * (len + 1));

  va_start(args, format);
  ptr += vsnprintf(ptr, (len + 1), format, args);
  va_end(args);
  ptr += snprintf(ptr, (len + 1) - (ptr - errorString), "\n");
  if (buf.length) {
    memcpy(ptr, buf.data, buf.length);
    ptr += buf.length;
  }
  *ptr = '\0';

  freeBuffer(&buf);
}

const char *getErrorString(void) {
  return errorString;
}

void clearErrorString(void) {
  free(errorString);
  errorString = NULL;
}

void setErrorContextProvider(void (*contextProvider)(Buffer *)) {
  errorContextProvider = contextProvider;
}

NORETURN void assertionError(const char *message) {
  panic("assertion error: %s", message);
}

void saveCurrentErrorString(void) {
  clearSavedErrorString();
  savedErrorString = errorString;
  errorString = NULL;
}

const char *getSavedErrorString(void) {
  return savedErrorString;
}

void clearSavedErrorString(void) {
  free(savedErrorString);
  savedErrorString = NULL;
}

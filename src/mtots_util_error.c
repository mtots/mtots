#include "mtots_util_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

static void (*errorContextProvider)(StringBuilder *);
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
    StringBuilder sb;
    initStringBuilder(&sb);
    errorContextProvider(&sb);
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "MTOTS_RT", "%s", sbstring(&sb)->buffer);
#else
    fprintf(stderr, "%.*s", (int)sb.length, sb.buffer);
#endif
    freeStringBuilder(&sb);
  }
  abort();
}

/* Potentially recoverable error. */
void runtimeError(const char *format, ...) {
  size_t len = 0;
  va_list args;
  char *ptr;
  StringBuilder sb;

  initStringBuilder(&sb);

  if (errorContextProvider) {
    errorContextProvider(&sb);
  }

  va_start(args, format);
  len += vsnprintf(NULL, 0, format, args);
  va_end(args);
  len++; /* '\n' */

  len += sb.length;

  ptr = errorString = (char *)realloc(errorString, sizeof(char) * (len + 1));

  va_start(args, format);
  ptr += vsnprintf(ptr, (len + 1), format, args);
  va_end(args);
  ptr += snprintf(ptr, (len + 1) - (ptr - errorString), "\n");
  if (sb.length) {
    memcpy(ptr, sb.buffer, sb.length);
    ptr += sb.length;
  }
  *ptr = '\0';

  freeStringBuilder(&sb);
}

const char *getErrorString(void) {
  return errorString;
}

void clearErrorString(void) {
  free(errorString);
  errorString = NULL;
}

void setErrorContextProvider(void (*contextProvider)(StringBuilder *)) {
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

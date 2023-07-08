#include "mtots_util_printf.h"

#include <stdarg.h>

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <stdio.h>
#endif

void oprintln(const char *format, ...) {
  va_list args;
  va_start(args, format);
#ifdef __ANDROID__
  __android_log_vprint(ANDROID_LOG_INFO, "MTOTS_RT", format, args);
#else
  vprintf(format, args);
  printf("\n");
#endif
  va_end(args);
}

void eprintln(const char *format, ...) {
  va_list args;
  va_start(args, format);
#ifdef __ANDROID__
  __android_log_vprint(ANDROID_LOG_ERROR, "MTOTS_RT", format, args);
#else
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
#endif
  va_end(args);
}

/*
void eprint(const char *format, ...) {
  va_list args;
  va_start(args, format);
#ifdef __ANDROID__
  __android_log_vprint(ANDROID_LOG_ERROR, "MTOTS_RT", format, args);
#else
  vfprintf(stderr, format, args);
#endif
  va_end(args);
}
*/

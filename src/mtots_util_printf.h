#ifndef mtots_util_printf_h
#define mtots_util_printf_h

#include "mtots_util_printflike.h"

/**
 * Like printf, but will handle properly on platforms
 * that do not support STDOUT like Android.
 *
 * Also adds a newline at the end.
 */
void oprintln(const char *format, ...) MTOTS_PRINTFLIKE(1, 2);

/**
 * Like fprintf(stderr, ...), but will handle properly on platforms
 * that do not support STDERR like Android.
 *
 * Also adds a newline at the end.
 */
void eprintln(const char *format, ...) MTOTS_PRINTFLIKE(1, 2);

#endif/*mtots_util_printf_h*/

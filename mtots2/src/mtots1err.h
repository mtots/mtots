#ifndef mtots_err_h
#define mtots_err_h

#include "mtots0common.h"

/* Flag a fatal error. The message is written to stderr and the program exits
 * with non-zero return code */
NORETURN void panic(const char *format, ...) MTOTS_PRINTFLIKE(1, 2);

/* Set a potentially recoverable error.
 * Pass an empty string to clear the error. */
void runtimeError(const char *format, ...) MTOTS_PRINTFLIKE(1, 2);

/* Gets the last error string set by runtimeError.
 * If the error string is empty (i.e. length zero), this function
 * returns NULL. This function will never return a string of length zero. */
const char *getErrorString(void);

#endif /*mtots_err_h*/

#ifndef mtots_util_error_h
#define mtots_util_error_h

#include "mtots_util_printflike.h"
#include "mtots_util_buffer.h"

/* Flag a fatal error. The message is written to stderr and the program exits
 * with non-zero return code */
NORETURN void panic(const char *format, ...) __printflike(1, 2);

/* Potentially recoverable error. */
void runtimeError(const char *format, ...) __printflike(1, 2);

/* Returns the current error string.
 * Returns NULL if runtimeError has never been called .*/
const char *getErrorString();

/* Clears the current error string so that 'getErrorString()' returns NULL */
void clearErrorString();

/* Adds a context provider that can add additional context to error messages.
 * The provider will be called for both 'panic' and 'runtimeError'. */
void setErrorContextProvider(void (*contextProvider)(Buffer*));

/* Basically a panic with a standandardized error message */
NORETURN void assertionError(const char *message);

/* Save the error string you would get with 'getErrorString()' */
void saveCurrentErrorString();

/* Get the erorr string you saved with 'saveCurrentErrorString()' */
const char *getSavedErrorString();

/* Clear the error string saved with 'saveCurrentErrorString()' */
void clearSavedErrorString();

#endif/*mtots_util_error_h*/

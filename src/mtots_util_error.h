#ifndef mtots_util_error_h
#define mtots_util_error_h

#include "mtots_util_sb.h"

/* Flag a fatal error. The message is written to stderr and the program exits
 * with non-zero return code */
NORETURN void panic(const char *format, ...) MTOTS_PRINTFLIKE(1, 2);

/* Potentially recoverable error. */
void runtimeError(const char *format, ...) MTOTS_PRINTFLIKE(1, 2);

/* Set the exact error string that can be retrieved with getErrorString
 * unlike runtimeERror, setErrorString will ignore the current
 * contextProvider. */
void setErrorString(const char *newErrorString);

/* Returns the current error string.
 * Returns NULL if runtimeError has never been called .*/
const char *getErrorString(void);

/* Clears the current error string so that 'getErrorString()' returns NULL */
void clearErrorString(void);

/* Adds a context provider that can add additional context to error messages.
 * The provider will be called for both 'panic' and 'runtimeError'. */
void setErrorContextProvider(void (*contextProvider)(StringBuilder *));

/* Basically a panic with a standandardized error message */
NORETURN void assertionError(const char *message);

/* Save the error string you would get with 'getErrorString()' */
void saveCurrentErrorString(void);

/* Get the erorr string you saved with 'saveCurrentErrorString()' */
const char *getSavedErrorString(void);

/* Clear the error string saved with 'saveCurrentErrorString()' */
void clearSavedErrorString(void);

#endif /*mtots_util_error_h*/

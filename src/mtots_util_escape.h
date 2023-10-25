#ifndef mtots_util_escape_h
#define mtots_util_escape_h

/* Some utilities for dealing with strings */

#include "mtots_util_sb.h"

typedef struct StringEscapeOptions {
  ubool jsonSafe;
  ubool shorthandControlCodes; /* Use shorthands for common control codes (e.g. '\n') */
  ubool tryUnicode;            /* Try to use unicode escapes */
} StringEscapeOptions;

/* Initializes StringEscapeOptions with defaults */
void initStringEscapeOptions(StringEscapeOptions *opts);

Status escapeString(
    StringBuilder *out,
    const char *str,
    size_t length,
    StringEscapeOptions *givenOpts);

/* Takes in an escaped string (terminated with a '"') and computes its unescaped
 * version
 * When NULL is passed to out* parameters, they are ignored
 * outLen: length of the final string not including the null terminator
 * outBytes: the buffer to write out the final string */
Status unescapeString(
    StringBuilder *out,
    const char *str,
    const char *quote,
    size_t quoteLen);

#endif /*mtots_util_escape_h*/

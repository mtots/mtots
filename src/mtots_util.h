#ifndef mtots_util_h
#define mtots_util_h

/* Collection of self contained utilities */
/* TODO: make `panic` and `runtimeError` also self contained */

/* Utilities that would be useful in any C program */
#include "mtots_util_string.h"
#include "mtots_util_error.h"
#include "mtots_util_unicode.h"
#include "mtots_util_escape.h"
#include "mtots_util_readfile.h"
#include "mtots_util_writefile.h"
#include "mtots_util_buffer.h"
#include "mtots_util_base64.h"
#include "mtots_util_random.h"
#include "mtots_util_number.h"
#include "mtots_util_printf.h"
#include "mtots_util_fs.h"

/* Other generally useful utilities */
#include "mtots_util_color.h"
#include "mtots_util_vector.h"
#include "mtots_util_matrix.h"
#include "mtots_util_rect.h"

#endif/*mtots_util_h*/

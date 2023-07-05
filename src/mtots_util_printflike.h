#ifndef mtots_util_printflike_h
#define mtots_util_printflike_h

#ifndef __printflike
#if MTOTS_USE_PRINTFLIKE
#include <sys/cdefs.h>
#else
#define __printflike(a,b)
#endif
#endif

#endif/*mtots_util_printflike_h*/

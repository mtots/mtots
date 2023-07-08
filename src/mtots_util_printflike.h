#ifndef mtots_util_printflike_h
#define mtots_util_printflike_h

#ifdef __GNUC__
#define MTOTS_PRINTFLIKE(n,m) __attribute__((format(printf,n,m)))
#else
#define MTOTS_PRINTFLIKE(n,m)
#endif /* __GNUC__ */

#endif/*mtots_util_printflike_h*/

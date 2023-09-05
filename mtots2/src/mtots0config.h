#ifndef mtots0config_h
#define mtots0config_h

/****************************************************************
 * DEBUG FLAGS
 ****************************************************************/

#ifndef MTOTS_DEBUG_MEMORY_LEAK
#define MTOTS_DEBUG_MEMORY_LEAK 0
#endif

/****************************************************************
 * Compiler Features
 ****************************************************************/

/* #if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900) */
#if __cplusplus >= 201103L /* C++11 and above */
#define NORETURN [[noreturn]]
#elif __STDC_VERSION__ >= 201112L /* C11 and above  */
#define NORETURN _Noreturn
#elif __GNUC__
#define NORETURN __attribute__((noreturn))
#else /* Assume C89 only */
#define NORETURN
#endif /* __cplusplus */

#if __cplusplus >= 201703L /* C++17 and above */
#define NODISCARD [[nodiscard]]
#elif __GNUC__
#define NODISCARD __attribute__((warn_unused_result))
#else /* Assume C89 only */
#define NODISCARD
#endif /* __cplusplus */

#ifdef __GNUC__
#define MTOTS_PRINTFLIKE(n, m) __attribute__((format(printf, n, m)))
#else
#define MTOTS_PRINTFLIKE(n, m)
#endif /* __GNUC__ */

#ifdef __GNUC__
#define MTOTS_FALLTHROUGH __attribute__((fallthrough))
#else
#define MTOTS_FALLTHROUGH
#endif /* __GNUC__ */

#endif /*mtots0config_h*/

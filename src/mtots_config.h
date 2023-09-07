#ifndef mtots_config_h
#define mtots_config_h

#if defined(MTOTS_RELEASE) && MTOTS_RELEASE
#define DEBUG_STRESS_GC 0
#else
#define DEBUG_STRESS_GC 1
#endif

#define MAX_PATH_LENGTH 4096
#define MAX_ELIF_CHAIN_COUNT 64
#define MAX_IDENTIFIER_LENGTH 128
#define FREAD_BUFFER_SIZE 8192

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define _CRT_SECURE_NO_WARNINGS
#define PATH_SEP '\\'
#define PATH_SEP_STR "\\"
#define PATH_LIST_SEP ';'
#else
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#define PATH_LIST_SEP ':'
#endif

#define MTOTS_HOME_DIR_NAME ".mtots"
#define MTOTS_LIB_DIR_NAME "lib"
#define MTOTS_FILE_EXTENSION ".mtots"
#define MTOTS_PATH_VARIABLE_NAME "MTOTSPATH"

/****************************************************************
 * Language version
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

/****************************************************************
 * OS
 ****************************************************************/

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define OS_NAME "windows"
#define MTOTS_ASSUME_WINDOWS 1
#elif __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define OS_NAME "iphone"
#elif TARGET_OS_MAC
#define OS_NAME "macos"
#define MTOTS_ASSUME_MACOS 1
#endif
#elif __EMSCRIPTEN__
#define OS_NAME "emscripten"
#elif defined(__ANDROID__)
#define OS_NAME "android"
#elif __linux__
#define OS_NAME "linux"
#elif __unix__
#define OS_NAME "unix"
#elif defined(_POSIX_VERSION)
#define OS_NAME "posix"
#else
#define OS_NAME "unknown"
#endif

#ifndef MTOTS_ASSUME_POSIX
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#define MTOTS_ASSUME_POSIX 1
#endif
#endif

/****************************************************************
 * Other Compiler Features
 ****************************************************************/

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

#if __cplusplus >= 201703L /* C++17 and above */
#define NODISCARD [[nodiscard]]
#elif __GNUC__
#define NODISCARD __attribute__((warn_unused_result))
#else /* Assume C89 only */
#define NODISCARD
#endif /* __cplusplus */

#endif /*mtots_config_h*/

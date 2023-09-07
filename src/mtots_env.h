#ifndef mtots_env_h
#define mtots_env_h

#include "mtots_util_buffer.h"

/*
 * Returns a pointer to a statically allocated buffer containing
 * the path to the specified module.
 *
 * If the module could not be found, this function
 * returns NULL.
 */
const char *findMtotsModulePath(const char *moduleName);

/*
 * Update the last path that `findMtotsModulePath` will check.
 *
 * This path is expected to be the mtots executable - the root path
 * will be the `root` directory under the directory containing the executable.
 */
void registerMtotsExecutablePath(const char *argv0);

/*
 * Let the Mtots environment know the path to the main executable script.
 * This will allow checking paths relative to the main script before
 * checking any other paths.
 *
 * In particular the rule is:
 *   * if the basename of the script path is "main.mtots",
 *     the *parent* of the enclosing directory will be used as
 *     the root,
 *   * otherwise, the enclosign directory will be used as
 *     the root.
 */
void registerMtotsMainScriptPath(const char *scriptPath);

#endif /*mtots_env_h*/

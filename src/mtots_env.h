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
void registerMtotsExecutablePath(const char *exePath);

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

/*
 * Opens and loads an mtots archive (mtzip) file
 */
ubool openMtotsArchive(const char *filePath);

/*
 * Try to load a module from an archive.
 *
 * If such a file exists, memory is allocated on the heap.
 *
 * Returns false if there is no archive open.
 *
 * When (*data) is true, both (*data) and (*dataPath) must be freed
 * by the caller.
 */
ubool readMtotsModuleFromArchive(const char *moduleName, char **data, char **dataPath);

/*
 * Read file from the archive and store the contents in space allocated on the heap.
 */
ubool readFileFromMtotsArchive(const char *path, char **data, size_t *dataSize);

/*
 * Read file from the archive and append the contents to the given Buffer.
 */
ubool readBufferFromMtotsArchive(const char *path, Buffer *out);

#endif/*mtots_env_h*/

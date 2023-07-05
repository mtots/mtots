#ifndef mtots_util_fs_h
#define mtots_util_fs_h

/* File System Utilities */

#include "mtots_common.h"

/* Given a path to a file, tries to get the size of a file */
ubool getFileSize(const char *path, size_t *out);

/* Tests whether a given path points to a file that exists or not */
ubool isFile(const char *path);

/* Tests whether a given path points to a directory or not */
ubool isDirectory(const char *path);

/* List the names of all files in a directory.
 * The given callback will be called with the given userData and
 * each file name. */
ubool listDirectory(
  const char *dirpath,
  void *userData,
  ubool (*callback)(void *userData, const char *fileName));

/* Creates a directory. If `existOK` is true, error resulting
 * from the directory already existing is ignored */
ubool makeDirectory(const char *dirpath, ubool existOK);

#endif/*mtots_util_fs_h*/

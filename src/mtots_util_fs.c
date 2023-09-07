#include "mtots_common.h"

#if MTOTS_ASSUME_POSIX
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#elif MTOTS_ASSUME_WINDOWS
#include <Windows.h>
#endif

#include <stdio.h>
#include <string.h>

#include "mtots_util_error.h"
#include "mtots_util_fs.h"
#include "mtots_util_string.h"

ubool getFileSize(const char *path, size_t *out) {
#if MTOTS_ASSUME_POSIX
  struct stat st;
  if (stat(path, &st) != 0) {
    runtimeError("Failed to stat file %s", path);
    return UFALSE;
  }
  *out = st.st_size;
  return UTRUE;
#else
  /* Use stdio hack
   * TODO: for windows, use GetFileSizeEx */
  FILE *file = fopen(path, "rb");
  if (!file) {
    runtimeError("Failed to open file %s", path);
    return UFALSE;
  }
  fseek(file, 0, SEEK_END);
  *out = ftell(file);
  fclose(file);
  return UTRUE;
#endif
}

ubool isFile(const char *path) {
#if MTOTS_ASSUME_POSIX
  struct stat st;
  int status;
  status = stat(path, &st);
  return status == 0 && !!S_ISREG(st.st_mode);
#else
  /* TODO: Windows */
  FILE *file = fopen(path, "r");
  ubool result = !!file;
  if (file) {
    fclose(file);
  }
  return result;
#endif
}

ubool isDirectory(const char *path) {
#if MTOTS_ASSUME_POSIX
  struct stat st;
  int status;
  status = stat(path, &st);
  return status == 0 && !!S_ISDIR(st.st_mode);
#elif MTOTS_ASSUME_WINDOWS
  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && attr & FILE_ATTRIBUTE_DIRECTORY;
#else
  panic(
      "isDirectory(): Checking if a path is a directory on "
      "the current platform is not yet supported");
  return UFALSE;
#endif
}

ubool listDirectory(
    const char *dirpath,
    void *userData,
    ubool (*callback)(void *userData, const char *fileName)) {
#if MTOTS_ASSUME_POSIX
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(dirpath)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (!callback(userData, ent->d_name)) {
        closedir(dir);
        return UFALSE;
      }
    }
    closedir(dir);
  } else {
    runtimeError("Could not open path as a directory: %s", dirpath);
    return UFALSE;
  }
  return UTRUE;
#elif MTOTS_ASSUME_WINDOWS
  HANDLE hFind;
  WIN32_FIND_DATAA entry;
  Buffer query;
  initBuffer(&query);
  bprintf(&query, "%s\\*", dirpath);
  hFind = FindFirstFileA((char *)query.data, &entry);
  if (hFind == INVALID_HANDLE_VALUE) {
    freeBuffer(&query);
    runtimeError("Could not open path as a directory: %s", dirpath);
    return UFALSE;
  }
  do {
    if (!callback(userData, entry.cFileName)) {
      freeBuffer(&query);
      FindClose(hFind);
      return UFALSE;
    }
  } while (FindNextFileA(hFind, &entry));
  if (GetLastError() != ERROR_NO_MORE_FILES) {
    freeBuffer(&query);
    FindClose(hFind);
    runtimeError("Error while listing directory");
    return UFALSE;
  }
  freeBuffer(&query);
  FindClose(hFind);
  return UTRUE;
#else
  runtimeError(
      "Listing directories on the current platform is not yet supported");
  return UFALSE;
#endif
}

ubool makeDirectory(const char *dirpath, ubool existOK) {
#if MTOTS_ASSUME_POSIX
  errno = 0;
  if (mkdir(dirpath, 0777) == -1) {
    int err = errno;
    if (err == EEXIST && existOK) {
      /* ignore error in this case */
    } else {
      runtimeError("makeDirectory: %s", strerror(err));
      return UFALSE;
    }
  }
  return UTRUE;
#elif MTOTS_ASSUME_WINDOWS
  if (!CreateDirectoryA(dirpath, NULL)) {
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) {
      if (existOK) {
        /* ignore error in this case */
      } else {
        runtimeError("makeDirectory: already exists");
        return UFALSE;
      }
    } else if (err == ERROR_PATH_NOT_FOUND) {
      runtimeError("makeDirectory: path not found");
      return UFALSE;
    } else {
      runtimeError("makeDirectory: unknown error %d", err);
      return UFALSE;
    }
  }
  return UTRUE;
#else
  runtimeError("makeDirectory on the current platform is not yet supported");
  return UFALSE;
#endif
}

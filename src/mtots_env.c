#include "mtots_env.h"
#include "mtots_common.h"
#include "mtots_util_buffer.h"
#include "mtots_util_error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if MTOTS_ASSUME_WINDOWS
#include <libloaderapi.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#elif __linux__
#include <unistd.h>
#endif

#if MTOTS_ENABLE_ZIP
#include "miniz.h"
#endif

#define PATH_LIMIT 1024

/* TODO: allocate these dynamically.
 * I fear that this may be wasteful in memory */
static char scriptBuf[PATH_LIMIT];
static char exeRootBuf[PATH_LIMIT];
static char pathBuffer[PATH_LIMIT];
static char exePathBuf[PATH_LIMIT];

#if MTOTS_ENABLE_ZIP
static char *archivePath;
static mz_zip_archive archive;
static ubool archiveOpened;
#endif

static ubool canOpen(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file) {
    fclose(file);
    return UTRUE;
  }
  return UFALSE;
}

static ubool testPrefix(size_t prefixLen) {
  size_t totalLen;

  /* Try `module/path/__init__.mtots` */
  totalLen = prefixLen + strlen(PATH_SEP_STR "__init__"  MTOTS_FILE_EXTENSION);
  if (totalLen + 1 < PATH_LIMIT) {
    strcpy(pathBuffer + prefixLen, PATH_SEP_STR "__init__"  MTOTS_FILE_EXTENSION);
    if (canOpen(pathBuffer)) {
      return UTRUE;
    }
  }

  /* Try `module/path.mtots` */
  totalLen = prefixLen + strlen(MTOTS_FILE_EXTENSION);
  if (totalLen + 1 < PATH_LIMIT) {
    strcpy(pathBuffer + prefixLen, MTOTS_FILE_EXTENSION);
    if (canOpen(pathBuffer)) {
      return UTRUE;
    }
  }

  return UFALSE;
}

static ubool testRoot(size_t rootLen, const char *moduleName, size_t moduleNameLen) {
  size_t prefixLen = rootLen + 1 + moduleNameLen;
  if (prefixLen + 1 < PATH_LIMIT) {
    size_t i;
    pathBuffer[rootLen] = PATH_SEP;
    strcpy(pathBuffer + rootLen + 1, moduleName);
    pathBuffer[prefixLen] = '\0';
    for (i = rootLen; i < prefixLen; i++) {
      if (pathBuffer[i] == '.') {
        pathBuffer[i] = PATH_SEP;
      }
    }
    return testPrefix(prefixLen);
  }
  return UFALSE;
}

/*
static ubool testPartialRoot(
    size_t rootPrefixLen,
    const char *rootSuffix,
    const char *moduleName,
    size_t moduleNameLen) {
  size_t rootSuffixLen = strlen(rootSuffix);
  size_t rootLen = rootPrefixLen + rootSuffixLen;
  if (rootLen + 1 < PATH_LIMIT) {
    strcpy(pathBuffer + rootPrefixLen, rootSuffix);
    return testRoot(rootLen, moduleName, moduleNameLen);
  }
  return UFALSE;
}
*/

const char *findMtotsModulePath(const char *moduleName) {
  size_t moduleNameLen = strlen(moduleName);

  /* Check directory relative to the main script */
  strcpy(pathBuffer, scriptBuf);
  if (testRoot(strlen(scriptBuf), moduleName, moduleNameLen)) {
    return pathBuffer;
  }

  /* Check directories indicated by $MTOTSPATH */
  {
    const char *mtotsPath = getenv(MTOTS_PATH_VARIABLE_NAME);
#ifdef MTOTSPATH_FALLBACK
    if (!mtotsPath || !mtotsPath[0]) {
      mtotsPath = MTOTSPATH_FALLBACK;
    }
#endif
    if (mtotsPath) {
      size_t i = 0;
      while (mtotsPath[i] != '\0') {
        size_t start = i, rootLen;
        while (mtotsPath[i] != PATH_LIST_SEP && mtotsPath[i] != '\0') {
          i++;
        }
        rootLen = i - start;
        if (rootLen + 1 < PATH_LIMIT) {
          memcpy(pathBuffer, mtotsPath + start, rootLen);
          pathBuffer[rootLen] = '\0';
          if (testRoot(rootLen, moduleName, moduleNameLen)) {
            return pathBuffer;
          }
        }
      }
    }
  }

  /* Check directory relative to the mtots executable */
  strcpy(pathBuffer, exeRootBuf);
  if (testRoot(strlen(exeRootBuf), moduleName, moduleNameLen)) {
    return pathBuffer;
  }

  /*
   * Check fixed directories under $HOME/git/mtots
   * (%USERPROFILE%/git/mtots on windows)
   */
  /*
  {
    const char *homePath = getenv("HOME");
    if (!homePath) {
      homePath = getenv("USERPROFILE");
    }
    if (homePath && homePath[0] != '\0') {
      size_t homeLen = strlen(homePath);

      if (homeLen + 1 < PATH_LIMIT) {
        ubool found;
        strcpy(pathBuffer, homePath);
        found =
          testPartialRoot(
            homeLen,
            PATH_SEP_STR "git" PATH_SEP_STR "mtots" PATH_SEP_STR "root",
            moduleName,
            moduleNameLen) ||
          testPartialRoot(
            homeLen,
            PATH_SEP_STR "git" PATH_SEP_STR "mtots" PATH_SEP_STR "apps",
            moduleName,
            moduleNameLen);
        return found ? pathBuffer : NULL;
      }
    }
  }
  */

  return NULL;
}

/* Basically https://stackoverflow.com/questions/933850 */
static const char *getMtotsExecutablePath(const char *argv0) {
#if MTOTS_ASSUME_WINDOWS
  /* TODO: handle cases where buffer is too small */
  GetModuleFileName(NULL, exePathBuf, PATH_LIMIT);
  exePathBuf[PATH_LIMIT - 1] = '\0';
  return exePathBuf;
#elif __APPLE__
  uint32_t bufsize;
  if (_NSGetExecutablePath(exePathBuf, &bufsize) != 0) {
    /* buffer is too small */
    return NULL;
  }
  return exePathBuf;
#elif __linux__
  if (readlink("/proc/self/exe", exePathBuf, PATH_LIMIT - 1) == -1) {
    /* Failed to read, maybe buffer too small */
    return NULL;
  }
  exePathBuf[PATH_LIMIT - 1] = '\0';
  return exePathBuf;
#else
  /* TODO: Try other ways to figure this out */
  (void)exePathBuf;
  return NULL;
#endif
}

void registerMtotsExecutablePath(const char *argv0) {
  const char *exePath = getMtotsExecutablePath(argv0);
  size_t exePathLen, rootLen;
  if (!exePath) {
    return;
  }
  exePathLen = strlen(exePath);
  if (exePathLen + strlen("/root") >= PATH_LIMIT) {
    return; /* too long */
  }
  strcpy(exeRootBuf, exePath);
  for (rootLen = exePathLen; rootLen > 0 && exeRootBuf[rootLen - 1] != PATH_SEP; rootLen--) {
    exeRootBuf[rootLen - 1] = '\0';
  }
  if (rootLen) {
    strcpy(exeRootBuf + rootLen, "root");
  } else {
    strcpy(exeRootBuf, "." PATH_SEP_STR "root");
  }
}

void registerMtotsMainScriptPath(const char *scriptPath) {
  size_t scriptPathLen = strlen(scriptPath), rootLen;
  if (scriptPathLen >= PATH_LIMIT) {
    return; /* too long */
  }
  strcpy(scriptBuf, scriptPath);
  for (rootLen = scriptPathLen; rootLen > 0 && scriptBuf[rootLen - 1] != PATH_SEP; rootLen--);
  if (strcmp(scriptBuf + rootLen, "main.mtots") == 0) {
    /* If the name of the script is 'main.mtots', we need the parent of the
     * enclosing directory */
    strcpy(scriptBuf + rootLen, "..");
  } else {
    /* otherwise, we use the enclosing directory */
    if (rootLen) {
      scriptBuf[rootLen] = '\0';
    } else {
      strcpy(scriptBuf, ".");
    }
  }
}

#if MTOTS_ENABLE_ZIP
static ubool minizErr(mz_zip_archive *za, const char *tag) {
  runtimeError("miniz %s: %s", tag, mz_zip_get_error_string(mz_zip_get_last_error(za)));
  return UFALSE;
}

ubool openMtotsArchive(const char *filePath) {
  if (!mz_zip_reader_init_file(&archive, filePath, 0)) {
    switch (mz_zip_peek_last_error(&archive)) {
      case MZ_ZIP_FILE_NOT_FOUND:
        runtimeError("file not found while trying to open Mtots archive: %s", filePath);
        return UFALSE;
      case MZ_ZIP_FILE_OPEN_FAILED:
        runtimeError("Could not open (Mtots archive) file: %s", filePath);
        return UFALSE;
      default:
        break;
    }
    return minizErr(&archive, "mz_zip_reader_init_file");
  }
  {
    size_t filePathLen = strlen(filePath);
    archivePath = (char*)malloc(filePathLen + 1);
    archivePath[filePathLen] = '\0';
    strcpy(archivePath, filePath);
  }
  archiveOpened = UTRUE;
  return UTRUE;
}

ubool readMtotsModuleFromArchive(const char *moduleName, char **data, char **dataPath) {
  char *fullPath = NULL;
  char path[PATH_LIMIT];
  size_t i, nameLen = strlen(moduleName), bufferSize, fullPathLen;
  int fileIndex;
  mz_zip_archive_file_stat stat;
  char *buffer;

  *data = NULL;
  *dataPath = NULL;
  if (!archiveOpened) {
    return UTRUE;
  }

  if (nameLen + strlen("/__init__.mtots") >= PATH_LIMIT) {
    return UTRUE; /* too long */
  }
  strcpy(path, moduleName);
  for (i = 0; i < nameLen; i++) {
    if (path[i] == '.') {
      path[i] = '/';
    }
  }
  strcpy(path + nameLen, ".mtots");
  fileIndex = mz_zip_reader_locate_file(&archive, path, NULL, 0);
  if (fileIndex < 0) {
    strcpy(path + nameLen, "/__init__.mtots");
    fileIndex = mz_zip_reader_locate_file(&archive, path, NULL, 0);
    if (fileIndex < 0) {
      return UTRUE; /* file not found */
    }
  }
  if (!mz_zip_reader_file_stat(&archive, fileIndex, &stat)) {
    return minizErr(&archive, "mz_zip_reader_file_stat");
  }
  bufferSize = stat.m_uncomp_size + 1; /* extra byte for the null terminator */
  buffer = (char*)malloc(bufferSize);
  if (!mz_zip_reader_extract_to_mem(&archive, fileIndex, buffer, bufferSize, 0)) {
    free((void*)buffer);
    return minizErr(&archive, "mz_zip_reader_extract_to_mem");
  }
  buffer[bufferSize - 1] = '\0';

  fullPathLen = strlen(archivePath) + 1 + strlen(path) + 1;
  fullPath = (char*)malloc(fullPathLen);
  strcpy(fullPath, archivePath);
  strcpy(fullPath + strlen(fullPath), "!");
  strcpy(fullPath + strlen(fullPath), path);

  *data = buffer;
  *dataPath = fullPath;

  return UTRUE;
}

ubool readFileFromMtotsArchive(const char *path, char **data, size_t *dataSize) {
  int fileIndex;

  fileIndex = mz_zip_reader_locate_file(&archive, path, NULL, 0);
  if (fileIndex < 0) {
    runtimeError("File %s not found in archive", path);
    return UFALSE;
  }

  *data = (char*)mz_zip_reader_extract_to_heap(&archive, fileIndex, dataSize, 0);
  if (!*data) {
    return minizErr(&archive, "mz_zip_reader_extract_to_heap");
  }

  return UTRUE;
}

ubool readBufferFromMtotsArchive(const char *path, Buffer *out) {
  int fileIndex;
  mz_zip_archive_file_stat stat;
  size_t outStartSize = out->length;
  u8 *dataStart;

  fileIndex = mz_zip_reader_locate_file(&archive, path, NULL, 0);
  if (fileIndex < 0) {
    runtimeError("File %s not found in archive", path);
    return UFALSE;
  }

  if (!mz_zip_reader_file_stat(&archive, fileIndex, &stat)) {
    return minizErr(&archive, "mz_zip_reader_file_stat");
  }
  bufferSetLength(out, outStartSize + stat.m_uncomp_size);
  dataStart = out->data + outStartSize;

  if (!mz_zip_reader_extract_to_mem(&archive, fileIndex, dataStart, stat.m_uncomp_size, 0)) {
    return minizErr(&archive, "mz_zip_reader_extract_to_mem");
  }

  return UTRUE;
}

#else
ubool openMtotsArchive(const char *filePath) {
  runtimeError("Mtots archive support not enabled");
  return UFALSE;
}

ubool readMtotsModuleFromArchive(const char *moduleName, char **data, char **dataPath) {
  *data = NULL;
  *dataPath = NULL;
  return UTRUE;
}

ubool readFileFromMtotsArchive(const char *path, char **data, size_t *dataSize) {
  runtimeError("Mtots archive support not enabled");
  return UFALSE;
}

ubool readBufferFromMtotsArchive(const char *path, Buffer *out) {
  runtimeError("Mtots archive support not enabled");
  return UFALSE;
}

#endif

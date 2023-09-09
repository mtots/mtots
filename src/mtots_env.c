#include "mtots_env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_common.h"
#include "mtots_util_buffer.h"
#include "mtots_util_error.h"

#if MTOTS_ASSUME_WINDOWS
#include <Windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#elif __linux__
#include <unistd.h>
#endif

#define PATH_LIMIT 1024

/* TODO: allocate these dynamically.
 * I fear that this may be wasteful in memory */
static char scriptBuf[PATH_LIMIT];
static char exeRootBuf[PATH_LIMIT];
static char pathBuffer[PATH_LIMIT];
static char exePathBuf[PATH_LIMIT];

static ubool canOpen(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file) {
    fclose(file);
    return STATUS_OK;
  }
  return STATUS_ERROR;
}

static ubool testPrefix(size_t prefixLen) {
  size_t totalLen;

  /* Try `module/path/__init__.mtots` */
  totalLen = prefixLen + strlen(PATH_SEP_STR "__init__" MTOTS_FILE_EXTENSION);
  if (totalLen + 1 < PATH_LIMIT) {
    strcpy(pathBuffer + prefixLen, PATH_SEP_STR "__init__" MTOTS_FILE_EXTENSION);
    if (canOpen(pathBuffer)) {
      return STATUS_OK;
    }
  }

  /* Try `module/path.mtots` */
  totalLen = prefixLen + strlen(MTOTS_FILE_EXTENSION);
  if (totalLen + 1 < PATH_LIMIT) {
    strcpy(pathBuffer + prefixLen, MTOTS_FILE_EXTENSION);
    if (canOpen(pathBuffer)) {
      return STATUS_OK;
    }
  }

  return STATUS_ERROR;
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
  return STATUS_ERROR;
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
  return STATUS_ERROR;
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
  for (rootLen = scriptPathLen; rootLen > 0 && scriptBuf[rootLen - 1] != PATH_SEP; rootLen--)
    ;
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

#include "mtots_vm.h"
#include "mtots_repl.h"
#include "mtots_env.h"

#if defined(__IPHONEOS__) || defined(__TVOS__)
#include "SDL.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ubool endsWith(const char *string, const char *suffix) {
  size_t stringLen = strlen(string), suffixLen = strlen(suffix);
  if (suffixLen > stringLen) {
    return UFALSE;
  }
  return strcmp(string + (stringLen - suffixLen), suffix) == 0;
}

static ubool getModuleNameFromArchivePath(const char *path, char **out) {
  size_t start = strlen(path), end = start;
  char *moduleName;

  for (; start > 0 && path[start - 1] != PATH_SEP; start--);
  for (; end > 0 && path[end - 1] != '.'; end--);

  if (end == 0) {
    runtimeError("Could not infer module name from archive name");
    return UFALSE;
  }

  end--;
  moduleName = (char*)malloc(end - start + strlen(".main") + 1);
  memcpy(moduleName, path + start, end - start);
  strcpy(moduleName + (end - start), ".main");
  *out = moduleName;
  return UTRUE;
}

ubool runMainModule(int argc, const char **argv) {
  String *mainModuleName;
  registerArgs(argc - 1, argv + 1);
  mainModuleName = internCString("__main__");
  push(STRING_VAL(mainModuleName));

  if (endsWith(argv[1], ".zip") || endsWith(argv[1], ".mtzip")) {
    /* We want to execute an mtots zip archive */
    char *data, *dataPath, *moduleNameCString;

    if (!openMtotsArchive(argv[1])) {
      return UFALSE;
    }

    if (!getModuleNameFromArchivePath(argv[1], &moduleNameCString)) {
      return UFALSE;
    }

    if (!readMtotsModuleFromArchive(moduleNameCString, &data, &dataPath)) {
      free(moduleNameCString);
      return UFALSE;
    }
    if (!data) {
      runtimeError(
        "File module %s not found in archive (%s)",
        moduleNameCString, argv[1]);
      free(moduleNameCString);
      return UFALSE;
    }
    free(moduleNameCString);

    setIsArchiveScript(UTRUE);
    if (!importModuleWithPathAndSource(mainModuleName, dataPath, data, UTRUE, UTRUE)) {
      return UFALSE;
    }
  } else if (!importModuleWithPath(mainModuleName, argv[1])) {
    return UFALSE;
  }

  if (vm.runOnFinish) {
    push(CFUNCTION_VAL(vm.runOnFinish));
    if (!callFunction(0)) {
      return UFALSE;
    }
    pop();
  }

  pop(); /* mainModuleName */
  return UTRUE;
}

int mtotsMain(int argc, const char *argv[]) {
#ifdef __EMSCRIPTEN__
  const char *fakeArgv[2] = {
    "",
    MTOTS_WEB_START_SCRIPT
  };
  argc = 2;
  argv = fakeArgv;
#endif

  if (argc > 0) {
    registerMtotsExecutablePath(argv[0]);
    if (argc > 1) {
      registerMtotsMainScriptPath(argv[1]);
    }
  }
  initVM();

  if (argc == 1) {
    repl();
  } else if (argc > 1) {
    if (!runMainModule(argc, argv)) {
      if (getErrorString()) {
        fprintf(stderr, "%s", getErrorString());
      } else {
        fprintf(stderr, "(runtime-error, but no error message set)\n");
      }
      exit(1);
    }
  } else {
    fprintf(stderr, "Usage: mtots [path]\n");
  }

  freeVM();
  return 0;
}

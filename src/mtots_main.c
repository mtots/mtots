#include "mtots_main.h"

#include "mtots_env.h"
#include "mtots_repl.h"
#include "mtots_vm.h"

#if defined(__IPHONEOS__) || defined(__TVOS__)
#include "SDL.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ubool runMainModule(int argc, const char **argv) {
  String *mainModuleName;
  registerArgs(argc - 1, argv + 1);
  mainModuleName = internCString("__main__");
  push(STRING_VAL(mainModuleName));

  if (!importModuleWithPath(mainModuleName, argv[1])) {
    return STATUS_ERROR;
  }

  if (vm.runOnFinish) {
    push(CFUNCTION_VAL(vm.runOnFinish));
    if (!callFunction(0)) {
      return STATUS_ERROR;
    }
    pop();
  }

  pop(); /* mainModuleName */
  return STATUS_OK;
}

int mtotsMain(int argc, const char *argv[]) {
#ifdef __EMSCRIPTEN__
  const char *fakeArgv[2] = {
      "",
      MTOTS_WEB_START_SCRIPT};
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

#include "mtots_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_env.h"
#include "mtots_parser.h"
#include "mtots_vm.h"

/*
 * Runs the module specified by the given path with the given moduleName
 * Puts the result of running the module on the top of the stack
 * NOTE: Never cached (unlike importModule())
 */
Status importModuleWithPath(String *moduleName, const char *path) {
  void *source;
  if (!readFile(path, &source, NULL)) {
    return STATUS_ERROR;
  }
  return importModuleWithPathAndSource(moduleName, path, (char *)source, NULL, (char *)source);
}

/* NOTE: 'freePath' and 'freeSource' should match 'path' and 'source'.
 * These should only be provided if 'path' or 'source' should be freed
 * after use. Requiring that they be specified twice is to work around
 * the fact that 'path' and 'source' are 'const char*'. */
Status importModuleWithPathAndSource(
    String *moduleName, const char *path, const char *source,
    char *freePath, char *freeSource) {
  ObjClosure *closure;
  ObjThunk *thunk;
  ObjModule *module;
  String *pathStr;

  module = newModule(moduleName, UTRUE);
  push(valModule(module));
  mapSetN(&module->fields, "__name__", valString(moduleName));

  pathStr = internCString(path);
  if (freePath) {
    free((void *)freePath);
  }
  push(valString(pathStr));
  mapSetN(&module->fields, "__file__", valString(pathStr));
  pop(); /* pathStr */

  if (!parse((const char *)source, moduleName, &thunk)) {
    /* runtimeError("Failed to compile %s", path); */
    if (freeSource) {
      free((void *)freeSource);
    }
    return STATUS_ERROR;
  }
  if (freeSource) {
    free((void *)freeSource);
  }

  push(valThunk(thunk));
  closure = newClosure(thunk, module);
  pop(); /* function */

  push(valClosure(closure));

  if (callFunction(0)) {
    pop(); /* return value from run */

    push(valModule(module));

    /* We need to copy all fields of the instance to the class so
     * that method calls will properly call the functions in the module */
    mapAddAll(&module->fields, &module->klass->methods);

    pop(); /* module */

    /* the module we pushed in the beginning is still on the stack */
    return STATUS_OK;
  }
  return STATUS_ERROR;
}

static Status importModuleNoCache(String *moduleName) {
  Value nativeModuleThunkValue;

  /* Check for a native module with the given name */
  if (mapGetStr(&vm.nativeModuleThunks, moduleName, &nativeModuleThunkValue)) {
    ObjModule *module = NULL;
    Value moduleValue;
    if (isCFunction(nativeModuleThunkValue)) {
      Value result = valNil(), *stackStart;
      CFunction *nativeModuleThunk;
      nativeModuleThunk = nativeModuleThunkValue.as.cfunction;
      module = newModule(moduleName, UFALSE);
      moduleValue = valModule(module);
      push(valModule(module));
      stackStart = vm.stackTop;
      if (!nativeModuleThunk->body(1, &moduleValue, &result)) {
        return STATUS_ERROR;
      }
      /* At this point, module should be at the top of the stack */
      if (vm.stackTop != stackStart) {
        panic(
            "Native module started with %d items on the stack, but "
            "ended with %d",
            (int)(stackStart - vm.stack),
            (int)(vm.stackTop - vm.stack));
      }
    } else {
      assertionError("importModuleNoCache (native module)");
    }

    /* We need to copy all fields of the instance to the class so
     * that method calls will properly call the functions in the module */
    mapAddAll(&module->fields, &module->klass->methods);

    return STATUS_OK;
  }

  {
    /* Otherwise, we're working with a script */
    const char *path = findMtotsModulePath(moduleName->chars);
    ubool result;
    if (path == NULL) {
      runtimeError("Could not find module %s", moduleName->chars);
      return STATUS_ERROR;
    }
    result = importModuleWithPath(moduleName, path);
    return result;
  }
}

/* Loads the module specified by the given moduleName
 * Puts the result of running the module on the top of the stack
 *
 * importModule will search the modules cache first, and
 * if not found, will call importModuleWithPath and add the
 * new entry into the cache
 */
Status importModule(String *moduleName) {
  Value module = valNil();
  if (mapGetStr(&vm.modules, moduleName, &module)) {
    if (!isModule(module)) {
      /* vm.modules table should only contain modules */
      assertionError("importModule (cached not a module)");
    }
    push(module);
    return STATUS_OK;
  }
  if (!importModuleNoCache(moduleName)) {
    return STATUS_ERROR;
  }

  /* if importModuleNoCache is successful, we should have
   * a module at TOS */
  if (!isModule(vm.stackTop[-1])) {
    assertionError("importModule (TOS is not a module)");
  }
  mapSetStr(&vm.modules, moduleName, vm.stackTop[-1]);
  return STATUS_OK;
}

/*
 * Utility function for native modules to use to ensure
 * that other natives modules are loaded.
 *
 * This function is like `importModule`, except:
 *  * the name is specified with `const char*` instead of `String*`.
 *  * the module is popped from the stack.
 */
Status importModuleAndPop(const char *moduleName) {
  String *moduleNameString = internCString(moduleName);
  push(valString(moduleNameString));
  if (!importModule(moduleNameString)) {
    return STATUS_ERROR;
  }
  pop(); /* module (from importModule) */
  pop(); /* moduleName */
  return STATUS_OK;
}

/**
 * Loads a module, and runs a function in the module with the given arguments.
 */
/*
ubool runFunctionInModule(
    String *moduleName,
    String *functionName,
    i16 argCount,
    Value *args,
    Value *out) {
  i16 i;
  if (!importModule(moduleName)) {
    return STATUS_ERROR;
  }
  for (i = 0; i < argCount; i++) {
    push(args[i]);
  }
  if (!callMethod(functionName, argCount)) {
    return STATUS_ERROR;
  }
  *out = pop();
  return STATUS_OK;
}
*/

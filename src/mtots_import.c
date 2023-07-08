#include "mtots_import.h"
#include "mtots_vm.h"
#include "mtots_parser.h"
#include "mtots_env.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Runs the module specified by the given path with the given moduleName
 * Puts the result of running the module on the top of the stack
 * NOTE: Never cached (unlike importModule())
 */
ubool importModuleWithPath(String *moduleName, const char *path) {
  void *source;
  if (!readFile(path, &source, NULL)) {
    return UFALSE;
  }
  return importModuleWithPathAndSource(moduleName, path, (char*)source, UFALSE, UTRUE);
}

ubool importModuleWithPathAndSource(
    String *moduleName, const char *path, const char *source,
    ubool freePath, ubool freeSource) {
  ObjClosure *closure;
  ObjThunk *thunk;
  ObjModule *module;
  String *pathStr;

  module = newModule(moduleName, UTRUE);
  push(MODULE_VAL(module));
  mapSetN(&module->fields, "__name__", STRING_VAL(moduleName));

  pathStr = internCString(path);
  if (freePath) {
    free((void*)path);
  }
  push(STRING_VAL(pathStr));
  mapSetN(&module->fields, "__file__", STRING_VAL(pathStr));
  pop(); /* pathStr */

  if (!parse((char*)source, moduleName, &thunk)) {
    /* runtimeError("Failed to compile %s", path); */
    if (freeSource) {
      free((void*)source);
    }
    return UFALSE;
  }
  if (freeSource) {
    free((void*)source);
  }

  push(THUNK_VAL(thunk));
  closure = newClosure(thunk, module);
  pop(); /* function */

  push(CLOSURE_VAL(closure));

  if (callClosure(closure, 0)) {
    pop(); /* return value from run */

    push(MODULE_VAL(module));

    /* We need to copy all fields of the instance to the class so
     * that method calls will properly call the functions in the module */
    mapAddAll(&module->fields, &module->klass->methods);

    pop(); /* module */

    /* the module we pushed in the beginning is still on the stack */
    return UTRUE;
  }
  return UFALSE;
}

static ubool importModuleNoCache(String *moduleName) {
  Value nativeModuleThunkValue;

  /* Check for a native module with the given name */
  if (mapGetStr(&vm.nativeModuleThunks, moduleName, &nativeModuleThunkValue)) {
    ObjModule *module = NULL;
    Value moduleValue;
    if (IS_CFUNCTION(nativeModuleThunkValue)) {
      Value result = NIL_VAL(), *stackStart;
      CFunction *nativeModuleThunk;
      nativeModuleThunk = AS_CFUNCTION(nativeModuleThunkValue);
      module = newModule(moduleName, UFALSE);
      moduleValue = MODULE_VAL(module);
      push(MODULE_VAL(module));
      stackStart = vm.stackTop;
      if (!nativeModuleThunk->body(1, &moduleValue, &result)) {
        return UFALSE;
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

    return UTRUE;
  }

#if MTOTS_ENABLE_ZIP
  {
    /* Check for a script in the archive */
    char *moduleData, *path;
    if (readMtotsModuleFromArchive(moduleName->chars, &moduleData, &path) && moduleData) {
      return importModuleWithPathAndSource(moduleName, path, moduleData, UTRUE, UTRUE);
    }
  }
#endif

  {
    /* Otherwise, we're working with a script */
    const char *path = findMtotsModulePath(moduleName->chars);
    ubool result;
    if (path == NULL) {
      runtimeError("Could not find module %s", moduleName->chars);
      return UFALSE;
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
ubool importModule(String *moduleName) {
  Value module = NIL_VAL();
  if (mapGetStr(&vm.modules, moduleName, &module)) {
    if (!IS_MODULE(module)) {
      /* vm.modules table should only contain modules */
      assertionError("importModule (cached not a module)");
    }
    push(module);
    return UTRUE;
  }
  if (!importModuleNoCache(moduleName)) {
    return UFALSE;
  }

  /* if importModuleNoCache is successful, we should have
   * a module at TOS */
  if (!IS_MODULE(vm.stackTop[-1])) {
    assertionError("importModule (TOS is not a module)");
  }
  mapSetStr(&vm.modules, moduleName, vm.stackTop[-1]);
  return UTRUE;
}

/*
 * Utility function for native modules to use to ensure
 * that other natives modules are loaded.
 *
 * This function is like `importModule`, except:
 *  * the name is specified with `const char*` instead of `String*`.
 *  * the module is popped from the stack.
 */
ubool importModuleAndPop(const char *moduleName) {
  String *moduleNameString = internCString(moduleName);
  push(STRING_VAL(moduleNameString));
  if (!importModule(moduleNameString)) {
    return UFALSE;
  }
  pop(); /* module (from importModule) */
  pop(); /* moduleName */
  return UTRUE;
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
    return UFALSE;
  }
  for (i = 0; i < argCount; i++) {
    push(args[i]);
  }
  if (!callMethod(functionName, argCount)) {
    return UFALSE;
  }
  *out = pop();
  return UTRUE;
}
*/

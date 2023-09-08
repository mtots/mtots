#include "mtots4module.h"

#include "mtots1err.h"
#include "mtots2map.h"
#include "mtots2native.h"

struct Module {
  Native native;
  Symbol *name;
  Map *scope;
};

void retainModule(Module *module) {
  retainObject((Object *)module);
}

void releaseModule(Module *module) {
  releaseObject((Object *)module);
}

Value moduleValue(Module *module) {
  return objectValue((Object *)module);
}

ubool isModule(Value value) {
  return isNative(value) && ((Native *)value.as.object)->cls == getModuleClass();
}

Module *asModule(Value value) {
  if (!isModule(value)) {
    panic("Expected Module but got %s", getValueKindName(value));
  }
  return (Module *)value.as.object;
}

Module *newModule(Symbol *moduleName, Map *scope) {
  Module *module = NEW_NATIVE(Module, getModuleClass());
  module->name = moduleName;
  module->scope = scope;
  retainMap(scope);
  return module;
}

Map *moduleScope(Module *module) {
  return module->scope;
}

static void freeModule(Object *object) {
  Module *module = (Module *)object;
  mapClear(module->scope);
  releaseMap(module->scope);
}

static CFunction *methods[] = {
    NULL,
};

static Class MODULE_CLASS = {
    "Module",       /* name */
    sizeof(Module), /* size */
    NULL,           /* constructor */
    freeModule,     /* destructor */
    NULL,           /* nativeStaticMethods */
    methods,        /* nativeInstanceMethods */
};

Class *getModuleClass(void) {
  initStaticClass(&MODULE_CLASS);
  return &MODULE_CLASS;
}

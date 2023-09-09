#include "mtots_class_class.h"

#include "mtots_vm.h"

static Status implClassGetName(i16 argCount, Value *args, Value *out) {
  ObjClass *cls = asClass(args[0]);
  *out = STRING_VAL(cls->name);
  return STATUS_OK;
}

static CFunction funcClassGetName = {implClassGetName, "getName", 1};

void initClassClass(void) {
  CFunction *staticMethods[] = {
      &funcClassGetName,
      NULL,
  };
  newBuiltinClass("Class", &vm.classClass, NULL, staticMethods);
}

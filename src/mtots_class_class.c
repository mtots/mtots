#include "mtots_class_class.h"

#include "mtots_vm.h"

static ubool implClassGetName(i16 argCount, Value *args, Value *out) {
  ObjClass *cls = asClass(args[0]);
  *out = STRING_VAL(cls->name);
  return UTRUE;
}

static TypePattern argsClassGetName[] = {
    {TYPE_PATTERN_CLASS},
};

static CFunction funcClassGetName = {
    implClassGetName, "getName", 1, 0, argsClassGetName};

void initClassClass(void) {
  CFunction *staticMethods[] = {
      &funcClassGetName,
      NULL,
  };
  newBuiltinClass("Class", &vm.classClass, TYPE_PATTERN_CLASS, NULL, staticMethods);
}

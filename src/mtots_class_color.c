#include "mtots_vm.h"

static ubool implColorStaticCall(i16 argc, Value *args, Value *out) {
  u8 red = AS_U8(args[0]);
  u8 green = AS_U8(args[1]);
  u8 blue = AS_U8(args[2]);
  u8 alpha = argc > 3 ? AS_U8(args[3]) : 255;
  *out = COLOR_VAL(newColor(red, green, blue, alpha));
  return UTRUE;
}

static CFunction funcColorStaticCall = {
  implColorStaticCall, "__call__", 3, 4, argsNumbers
};

static ubool implColorGetattr(i16 argc, Value *args, Value *out) {
  Color color = AS_COLOR(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.redString) {
    *out = NUMBER_VAL(color.red);
  } else if (name == vm.greenString) {
    *out = NUMBER_VAL(color.green);
  } else if (name == vm.blueString) {
    *out = NUMBER_VAL(color.blue);
  } else if (name == vm.alphaString) {
    *out = NUMBER_VAL(color.alpha);
  } else {
    runtimeError("Field %s not found in Color", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcColorGetattr = {
  implColorGetattr, "__getattr__", 1, 0, argsStrings,
};

void initColorClass() {
  CFunction *staticMethods[] = {
    &funcColorStaticCall,
    NULL,
  };
  CFunction *methods[] = {
    &funcColorGetattr,
    NULL,
  };
  newBuiltinClass(
    "Color",
    &vm.colorClass,
    TYPE_PATTERN_COLOR,
    methods,
    staticMethods);
}

#include "mtots4globals.h"

#include <stdio.h>

#include "mtots2string.h"

static Status implPrint(i16 argc, Value *argv, Value *out) {
  printValue(argv[0]);
  printf("\n");
  return STATUS_OK;
}

static CFunction funcPrint = {"print", 1, 0, implPrint};

static Status implRepr(i16 argc, Value *argv, Value *out) {
  String *string = newString("");
  reprValue(string, argv[0]);
  freezeString(string);
  *out = stringValue(string);
  return STATUS_OK;
}

static CFunction funcRepr = {"repr", 1, 0, implRepr};

Map *newGlobals(void) {
  CFunction *functions[] = {
      &funcPrint,
      &funcRepr,
      NULL,
  };
  CFunction **func;
  Map *map = newMap();
  for (func = functions; *func; func++) {
    mapSet(map,
           symbolValue(newSymbol((*func)->name)),
           cfunctionValue(*func));
  }
  return map;
}

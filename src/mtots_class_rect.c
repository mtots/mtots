#include "mtots_class_rect.h"

#include "mtots_vm.h"

static ubool implRectStaticCall(i16 argc, Value *args, Value *out) {
  double minX = AS_NUMBER(args[0]);
  double minY = AS_NUMBER(args[1]);
  double width = AS_NUMBER(args[2]);
  double height = AS_NUMBER(args[3]);
  *out = RECT_VAL(newRect(minX, minY, width, height));
  return UTRUE;
}

static CFunction funcRectStaticCall = {
  implRectStaticCall, "__call__", 4, 0, argsNumbers,
};

static ubool implRectGetattr(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.widthString) {
    *out = NUMBER_VAL(rect.width);
  } else if (name == vm.heightString) {
    *out = NUMBER_VAL(rect.height);
  } else if (name == vm.minXString) {
    *out = NUMBER_VAL(rect.minX);
  } else if (name == vm.minYString) {
    *out = NUMBER_VAL(rect.minY);
  } else if (name == vm.maxXString) {
    *out = NUMBER_VAL(rect.minX + rect.width);
  } else if (name == vm.maxYString) {
    *out = NUMBER_VAL(rect.minY + rect.height);
  } else {
    runtimeError("Field %s not found on Rect", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcRectGetattr = {
  implRectGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implRectWithMinX(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  rect.minX = AS_NUMBER(args[0]);
  *out = RECT_VAL(rect);
  return UTRUE;
}

static CFunction funcRectWithMinX = {
  implRectWithMinX, "withMinX", 1, 0, argsNumbers
};

static ubool implRectWithMaxX(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  rect.minX = AS_NUMBER(args[0]) - rect.width;
  *out = RECT_VAL(rect);
  return UTRUE;
}

static CFunction funcRectWithMaxX = {
  implRectWithMaxX, "withMaxX", 1, 0, argsNumbers
};

static ubool implRectWithMaxY(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  rect.minY = AS_NUMBER(args[0]) - rect.height;
  *out = RECT_VAL(rect);
  return UTRUE;
}

static CFunction funcRectWithMaxY = {
  implRectWithMaxY, "withMaxY", 1, 0, argsNumbers
};

static ubool implRectWithMinY(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  rect.minY = AS_NUMBER(args[0]);
  *out = RECT_VAL(rect);
  return UTRUE;
}

static CFunction funcRectWithMinY = {
  implRectWithMinY, "withMinY", 1, 0, argsNumbers
};

static ubool implRectWithWidth(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  rect.width = AS_NUMBER(args[0]);
  *out = RECT_VAL(rect);
  return UTRUE;
}

static CFunction funcRectWithWidth = {
  implRectWithWidth, "withWidth", 1, 0, argsNumbers
};

static ubool implRectWithHeight(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  rect.height = AS_NUMBER(args[0]);
  *out = RECT_VAL(rect);
  return UTRUE;
}

static CFunction funcRectWithHeight = {
  implRectWithHeight, "withHeight", 1, 0, argsNumbers
};

static ubool implRectRepr(i16 argc, Value *args, Value *out) {
  Rect rect = AS_RECT(args[-1]);
  Buffer buf;
  initBuffer(&buf);
  bputstr(&buf, "Rect(");
  bputnumber(&buf, rect.minX);
  bputstr(&buf, ", ");
  bputnumber(&buf, rect.minY);
  bputstr(&buf, ", ");
  bputnumber(&buf, rect.width);
  bputstr(&buf, ", ");
  bputnumber(&buf, rect.height);
  bputstr(&buf, ")");
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static CFunction funcRectRepr = { implRectRepr, "__repr__" };

void initRectClass(void) {
  CFunction *staticMethods[] = {
    &funcRectStaticCall,
    NULL,
  };
  CFunction *methods[] = {
    &funcRectGetattr,
    &funcRectWithMinX,
    &funcRectWithMinY,
    &funcRectWithMaxX,
    &funcRectWithMaxY,
    &funcRectWithWidth,
    &funcRectWithHeight,
    &funcRectRepr,
    NULL,
  };
  newBuiltinClass(
    "Rect",
    &vm.rectClass,
    TYPE_PATTERN_RECT,
    methods,
    staticMethods);
}

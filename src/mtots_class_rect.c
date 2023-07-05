#include "mtots_class_rect.h"

#include "mtots_vm.h"

NativeObjectDescriptor descriptorRect = {
  nopBlacken, nopFree, sizeof(ObjRect), "Rect"
};

Value RECT_VAL(ObjRect *rect) {
  return OBJ_VAL_EXPLICIT((Obj*)rect);
}

ObjRect *allocRect(Rect handle) {
  ObjRect *rect = NEW_NATIVE(ObjRect, &descriptorRect);
  rect->handle = handle;
  return rect;
}

static ubool implRectStaticCall(i16 argc, Value *args, Value *out) {
  double minX = AS_NUMBER(args[0]);
  double minY = AS_NUMBER(args[1]);
  double width = AS_NUMBER(args[2]);
  double height = AS_NUMBER(args[3]);
  *out = RECT_VAL(allocRect(newRect(minX, minY, width, height)));
  return UTRUE;
}

static CFunction funcRectStaticCall = {
  implRectStaticCall, "__call__", 4, 0, argsNumbers,
};

static ubool implRectGetattr(i16 argc, Value *args, Value *out) {
  ObjRect *rect = AS_RECT(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.widthString) {
    *out = NUMBER_VAL(rect->handle.width);
  } else if (name == vm.heightString) {
    *out = NUMBER_VAL(rect->handle.height);
  } else if (name == vm.minXString) {
    *out = NUMBER_VAL(rect->handle.minX);
  } else if (name == vm.minYString) {
    *out = NUMBER_VAL(rect->handle.minY);
  } else if (name == vm.maxXString) {
    *out = NUMBER_VAL(rect->handle.minX + rect->handle.width);
  } else if (name == vm.maxYString) {
    *out = NUMBER_VAL(rect->handle.minY + rect->handle.height);
  } else {
    runtimeError("Field %s not found on Rect", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcRectGetattr = {
  implRectGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implRectSetattr(i16 argc, Value *args, Value *out) {
  ObjRect *rect = AS_RECT(args[-1]);
  String *name = AS_STRING(args[0]);
  double value = AS_NUMBER(args[1]);
  if (name == vm.widthString) {
    rect->handle.width = value;
  } else if (name == vm.heightString) {
    rect->handle.height = value;
  } else if (name == vm.minXString) {
    rect->handle.minX = value;
  } else if (name == vm.minYString) {
    rect->handle.minY = value;
  } else if (name == vm.maxXString) {
    rect->handle.minX = value - rect->handle.width;
  } else if (name == vm.maxYString) {
    rect->handle.minY = value - rect->handle.height;
  } else {
    runtimeError("Field %s not found on Rect", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static TypePattern argsRectSetattr[] = {
  { TYPE_PATTERN_STRING },
  { TYPE_PATTERN_NUMBER },
};

static CFunction funcRectSetattr = {
  implRectSetattr, "__setattr__", 2, 0, argsRectSetattr
};

static ubool implRectRepr(i16 argc, Value *args, Value *out) {
  ObjRect *rect = AS_RECT(args[-1]);
  Buffer buf;
  initBuffer(&buf);
  bputstr(&buf, "Rect(");
  bputnumber(&buf, rect->handle.minX);
  bputstr(&buf, ", ");
  bputnumber(&buf, rect->handle.minY);
  bputstr(&buf, ", ");
  bputnumber(&buf, rect->handle.width);
  bputstr(&buf, ", ");
  bputnumber(&buf, rect->handle.height);
  bputstr(&buf, ")");
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static CFunction funcRectRepr = { implRectRepr, "__repr__" };

static ubool implRectCopy(i16 argc, Value *args, Value *out) {
  ObjRect *rect = AS_RECT(args[-1]);
  ObjRect *other = AS_RECT(args[0]);
  rect->handle.minX = other->handle.minX;
  rect->handle.minY = other->handle.minY;
  rect->handle.width = other->handle.width;
  rect->handle.height = other->handle.height;
  return UTRUE;
}

static TypePattern argsRectCopy[] = {
  { TYPE_PATTERN_NATIVE, &descriptorRect },
};

static CFunction funcRectCopy = { implRectCopy, "copy", 1, 0, argsRectCopy };

void initRectClass() {
  CFunction *staticMethods[] = {
    &funcRectStaticCall,
    NULL,
  };
  CFunction *methods[] = {
    &funcRectGetattr,
    &funcRectSetattr,
    &funcRectRepr,
    &funcRectCopy,
    NULL,
  };
  newNativeClass(NULL, &descriptorRect, methods, staticMethods);
}

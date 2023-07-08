#include "mtots_vm.h"

static void sbBlacken(ObjNative *n) {}

static void sbFree(ObjNative *n) {
  ObjStringBuilder *sb = (ObjStringBuilder*)n;
  freeBuffer(&sb->buf);
}

ObjStringBuilder *newStringBuilder(void) {
  ObjStringBuilder *sb = NEW_NATIVE(ObjStringBuilder, &descriptorStringBuilder);
  initBuffer(&sb->buf);
  return sb;
}

static ubool implInstantiateStringBuilder(i16 argCount, Value *args, Value *out) {
  *out = OBJ_VAL_EXPLICIT((Obj*)newStringBuilder());
  return UTRUE;
}

static CFunction funcInstantiateStringBuilder = {
  implInstantiateStringBuilder, "__call__",
};

static ubool implStringBuilderClear(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = AS_STRING_BUILDER(args[-1]);
  bufferClear(&sb->buf);
  return UTRUE;
}

static CFunction funcStringBuilderClear = { implStringBuilderClear, "clear", };

static ubool implStringBuilderAdd(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = AS_STRING_BUILDER(args[-1]);
  String *string = AS_STRING(args[0]);
  bputstrlen(&sb->buf, string->chars, string->byteLength);
  return UTRUE;
}

static CFunction funcStringBuilderAdd = {
  implStringBuilderAdd, "add", 1, 0, argsStrings
};

static ubool implStringBuilderAddBase64(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = AS_STRING_BUILDER(args[-1]);
  ObjBuffer *buffer = AS_BUFFER(args[0]);
  if (!encodeBase64(buffer->handle.data, buffer->handle.length, &sb->buf)) {
    return UFALSE;
  }
  return UTRUE;
}

static TypePattern argsStringBuilderAddBase64 = {
  TYPE_PATTERN_BUFFER,
};

static CFunction funcSringBuilderAddBase64 = {
  implStringBuilderAddBase64, "addBase64", 1, 0, &argsStringBuilderAddBase64,
};

static ubool implStringBuilderBuild(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = (ObjStringBuilder*)AS_OBJ(args[-1]);
  *out = STRING_VAL(bufferToString(&sb->buf));
  return UTRUE;
}

static CFunction funcStringBuilderBuild = { implStringBuilderBuild, "build" };

void initStringBuilderClass(void) {
  CFunction *methods[] = {
    &funcStringBuilderClear,
    &funcStringBuilderAdd,
    &funcSringBuilderAddBase64,
    &funcStringBuilderBuild,
    NULL,
  };
  CFunction *staticMethods[] = {
    &funcInstantiateStringBuilder,
    NULL,
  };
  newNativeClass(NULL, &descriptorStringBuilder, methods, staticMethods);
}

NativeObjectDescriptor descriptorStringBuilder = {
  sbBlacken, sbFree,
  sizeof(ObjStringBuilder), "StringBuilder",
};

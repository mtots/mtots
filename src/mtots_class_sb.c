#include "mtots_vm.h"

static void sbBlacken(ObjNative *n) {}

static void sbFree(ObjNative *n) {
  ObjStringBuilder *sb = (ObjStringBuilder *)n;
  freeStringBuilder(&sb->handle);
}

ObjStringBuilder *newStringBuilder(void) {
  ObjStringBuilder *sb = NEW_NATIVE(ObjStringBuilder, &descriptorStringBuilder);
  initStringBuilder(&sb->handle);
  return sb;
}

static Status implInstantiateStringBuilder(i16 argCount, Value *args, Value *out) {
  *out = valObjExplicit((Obj *)newStringBuilder());
  return STATUS_OK;
}

static CFunction funcInstantiateStringBuilder = {
    implInstantiateStringBuilder,
    "__call__",
};

static Status implStringBuilderClear(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = asStringBuilder(args[-1]);
  sbclear(&sb->handle);
  return STATUS_OK;
}

static CFunction funcStringBuilderClear = {
    implStringBuilderClear,
    "clear",
};

static Status implStringBuilderAdd(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = asStringBuilder(args[-1]);
  String *string = asString(args[0]);
  sbputstrlen(&sb->handle, string->chars, string->byteLength);
  return STATUS_OK;
}

static CFunction funcStringBuilderAdd = {implStringBuilderAdd, "add", 1, 0};

static Status implStringBuilderAddBase64(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = asStringBuilder(args[-1]);
  ObjBuffer *buffer = asBuffer(args[0]);
  if (!encodeBase64(buffer->handle.data, buffer->handle.length, &sb->handle)) {
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcSringBuilderAddBase64 = {implStringBuilderAddBase64, "addBase64", 1};

static Status implStringBuilderBuild(i16 argCount, Value *args, Value *out) {
  ObjStringBuilder *sb = (ObjStringBuilder *)AS_OBJ_UNSAFE(args[-1]);
  *out = valString(sbstring(&sb->handle));
  return STATUS_OK;
}

static CFunction funcStringBuilderBuild = {implStringBuilderBuild, "build"};

ObjStringBuilder *asStringBuilder(Value value) {
  if (!isStringBuilder(value)) {
    panic("Expected StringBuilder but got %s", getKindName(value));
  }
  return (ObjStringBuilder *)value.as.obj;
}

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
    sbBlacken,
    sbFree,
    sizeof(ObjStringBuilder),
    "StringBuilder",
};

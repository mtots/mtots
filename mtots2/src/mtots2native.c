#include "mtots2native.h"

#include "mtots1err.h"
#include "mtots2string.h"

void retainNative(Native *native) {
  retainObject((Object *)native);
}

void releaseNative(Native *native) {
  releaseObject((Object *)native);
}

Value nativeValue(Native *native) {
  return objectValue((Object *)native);
}

ubool isNative(Value value) {
  return isObject(value) && value.as.object->type == OBJECT_NATIVE;
}

Native *asNative(Value value) {
  if (!isNative(value)) {
    panic("Expected Native value but got %s", getValueKindName(value));
  }
  return (Native *)value.as.object;
}

void reprNative(String *out, Native *native) {
  /* TOOD: allow customization */
  msprintf(out, "<%s native instance>", native->descriptor->name);
}

ubool eqNative(Native *a, Native *b) {
  /* TODO: allow customization */
  return a == b;
}

u32 hashNative(Native *a) {
  /* TODO: allow customization */
  panic("Native instances are not hashable");
}

#include "mtots2object.h"

typedef struct Native Native;

typedef struct NativeObjectDescriptor {
  const char *name;
  void (*destructor)(Native *);
} NativeObjectDescriptor;

struct Native {
  Object object;
  NativeObjectDescriptor *descriptor;
};

void retainNative(Native *native);
void releaseNative(Native *native);
Value nativeValue(Native *native);
ubool isNative(Value value);
Native *asNative(Value value);

void reprNative(String *out, Native *native);
ubool eqNative(Native *a, Native *b);
u32 hashNative(Native *a);

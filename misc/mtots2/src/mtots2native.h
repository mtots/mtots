#include "mtots2object.h"

#define NEW_NATIVE(type, cls) \
  ((type *)newNative(cls, sizeof(type)))

typedef struct Native {
  Object object;
  Class *cls;
} Native;

Native *newNative(Class *cls, size_t size);

void retainNative(Native *native);
void releaseNative(Native *native);
Value nativeValue(Native *native);
ubool isNative(Value value);
Native *asNative(Value value);

void reprNative(String *out, Native *native);
ubool eqNative(Native *a, Native *b);
u32 hashNative(Native *a);

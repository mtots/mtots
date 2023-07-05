#include "mtots_class_number.h"
#include "mtots_vm.h"

static ubool implNumberToU32(i16 argCount, Value *args, Value *out) {
  double value = AS_NUMBER(args[-1]);
  if (value < 0) {
    *out = NUMBER_VAL(0);
  } else if (value > (double)U32_MAX) {
    *out = NUMBER_VAL(U32_MAX);
  } else {
    *out = NUMBER_VAL((u32)value);
  }
  return UTRUE;
}

static CFunction funcNumberToU32 = {
  implNumberToU32, "toU32",
};

static ubool implNumberBase(i16 argCount, Value *args, Value *out) {
  i32 value = AS_I32(args[-1]);
  i32 base = AS_I32(args[0]);
  Buffer buf;
  size_t start, end;
  if (base > 36 || base < 2) {
    runtimeError("base > 36 and base < 2 are not supported (got %d)", (int)base);
    return UFALSE;
  }
  if (value == 0) {
    *out = STRING_VAL(internCString("0"));
    return UTRUE;
  }
  initBuffer(&buf);
  if (value < 0) {
    bputchar(&buf, '-');
  }
  start = buf.length;
  for (; value > 0; value /= base) {
    i32 digit = value % base;
    if (digit < 10) {
      bputchar(&buf, '0' + digit);
    } else {
      bputchar(&buf, 'A' + (digit - 10));
    }
  }
  end = buf.length;
  for (; start + 1 < end; start++, end--) {
    u8 tmp = buf.data[start];
    buf.data[start] = buf.data[end - 1];
    buf.data[end - 1] = tmp;
  }
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return UTRUE;
}

static CFunction funcNumberBase = { implNumberBase, "base", 1, 0, argsNumbers };

void initNumberClass() {
  CFunction *methods[] = {
    &funcNumberToU32,
    &funcNumberBase,
    NULL,
  };
  newBuiltinClass(
    "Number",
    &vm.numberClass,
    TYPE_PATTERN_NUMBER,
    methods,
    NULL);
}

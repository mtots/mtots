#include "mtots_class_number.h"

#include "mtots_vm.h"

static Status implNumberToU32(i16 argCount, Value *args, Value *out) {
  double value = asNumber(args[-1]);
  if (value < 0) {
    *out = valNumber(0);
  } else if (value > (double)U32_MAX) {
    *out = valNumber(U32_MAX);
  } else {
    *out = valNumber((u32)value);
  }
  return STATUS_OK;
}

static CFunction funcNumberToU32 = {
    implNumberToU32,
    "toU32",
};

static Status implNumberBase(i16 argCount, Value *args, Value *out) {
  i32 value = asI32(args[-1]);
  i32 base = asI32(args[0]);
  Buffer buf;
  size_t start, end;
  if (base > 36 || base < 2) {
    runtimeError("base > 36 and base < 2 are not supported (got %d)", (int)base);
    return STATUS_ERROR;
  }
  if (value == 0) {
    *out = valString(internCString("0"));
    return STATUS_OK;
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
  *out = valString(bufferToString(&buf));
  freeBuffer(&buf);
  return STATUS_OK;
}

static CFunction funcNumberBase = {implNumberBase, "base", 1};

void initNumberClass(void) {
  CFunction *methods[] = {
      &funcNumberToU32,
      &funcNumberBase,
      NULL,
  };
  newBuiltinClass(
      "Number",
      &vm.numberClass,
      methods,
      NULL);
}

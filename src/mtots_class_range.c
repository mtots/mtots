#include "mtots_vm.h"

static Status implRangeRepr(i16 argCount, Value *args, Value *out) {
  Range range = asRange(args[-1]);
  Buffer buf;
  initBuffer(&buf);
  bputstr(&buf, "Range(");
  bputnumber(&buf, range.start);
  bputchar(&buf, ',');
  bputnumber(&buf, range.stop);
  bputchar(&buf, ',');
  bputnumber(&buf, range.step);
  bputchar(&buf, ')');
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return STATUS_OK;
}

static CFunction funcRangeRepr = {implRangeRepr, "__repr__"};

void initRangeClass(void) {
  CFunction *methods[] = {
      &funcRangeRepr,
      NULL,
  };
  newBuiltinClass("Range", &vm.rangeClass, methods, NULL);
}

void initRangeIteratorClass(void) {
  newBuiltinClass("RangeIterator", &vm.rangeIteratorClass, NULL, NULL);
}

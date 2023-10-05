#include "mtots_vm.h"

static Status implRangeRepr(i16 argCount, Value *args, Value *out) {
  Range range = asRange(args[-1]);
  StringBuilder sb;
  initStringBuilder(&sb);
  sbputstr(&sb, "Range(");
  sbputnumber(&sb, range.start);
  sbputchar(&sb, ',');
  sbputnumber(&sb, range.stop);
  sbputchar(&sb, ',');
  sbputnumber(&sb, range.step);
  sbputchar(&sb, ')');
  *out = valString(sbstring(&sb));
  freeStringBuilder(&sb);
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

#include "mtots_vm.h"

ObjRange *asRange(Value value) {
  if (!isRange(value)) {
    panic("Expected Range but got %s", getKindName(value));
  }
  return (ObjRange *)AS_OBJ_UNSAFE(value);
}

ObjRangeIterator *asRangeIterator(Value value) {
  if (!isRangeIterator(value)) {
    panic("Expected RangeIterator but got %s", getKindName(value));
  }
  return (ObjRangeIterator *)AS_OBJ_UNSAFE(value);
}

ObjRange *newRange(double start, double stop, double step) {
  ObjRange *range = NEW_NATIVE(ObjRange, &descriptorRange);
  range->start = start;
  range->stop = stop;
  range->step = step;
  return range;
}

ObjRangeIterator *newRangeIterator(double next, double stop, double step) {
  ObjRangeIterator *range = NEW_NATIVE(ObjRangeIterator, &descriptorRangeIterator);
  range->next = next;
  range->stop = stop;
  range->step = step;
  return range;
}

static Status implRangeIter(i16 argCount, Value *args, Value *out) {
  ObjRange *range = asRange(args[-1]);
  *out = OBJ_VAL_EXPLICIT((Obj *)newRangeIterator(range->start, range->stop, range->step));
  return STATUS_OK;
}

static CFunction funcRangeIter = {implRangeIter, "__iter__"};

static Status implRangeRepr(i16 argCount, Value *args, Value *out) {
  ObjRange *range = asRange(args[-1]);
  Buffer buf;
  initBuffer(&buf);
  bputstr(&buf, "Range(");
  bputnumber(&buf, range->start);
  bputchar(&buf, ',');
  bputnumber(&buf, range->stop);
  bputchar(&buf, ',');
  bputnumber(&buf, range->step);
  bputchar(&buf, ')');
  *out = STRING_VAL(bufferToString(&buf));
  freeBuffer(&buf);
  return STATUS_OK;
}

static CFunction funcRangeRepr = {implRangeRepr, "__repr__"};

void initRangeClass(void) {
  CFunction *methods[] = {
      &funcRangeIter,
      &funcRangeRepr,
      NULL,
  };
  newNativeClass(NULL, &descriptorRange, methods, NULL);
}

static Status implRangeIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjRangeIterator *iter = asRangeIterator(args[-1]);
  if (iter->step > 0) {
    if (iter->next >= iter->stop) {
      *out = STOP_ITERATION_VAL();
      return STATUS_OK;
    }
  } else if (iter->next <= iter->stop) {
    *out = STOP_ITERATION_VAL();
    return STATUS_OK;
  }
  *out = NUMBER_VAL(iter->next);
  iter->next += iter->step;
  return STATUS_OK;
}

static CFunction funcRangeIteratorCall = {implRangeIteratorCall, "__call__"};

void initRangeIteratorClass(void) {
  CFunction *methods[] = {
      &funcRangeIteratorCall,
      NULL,
  };
  newNativeClass(NULL, &descriptorRangeIterator, methods, NULL);
}

NativeObjectDescriptor descriptorRange = {
    nopBlacken,
    nopFree,
    sizeof(ObjRange),
    "Range",
};

NativeObjectDescriptor descriptorRangeIterator = {
    nopBlacken,
    nopFree,
    sizeof(ObjRangeIterator),
    "RangeIterator",
};

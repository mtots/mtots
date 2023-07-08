#include "mtots_vm.h"

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

static ubool implFastRangeIter(i16 argCount, Value *args, Value *out) {
  FastRange range = AS_FAST_RANGE(args[-1]);
  *out = OBJ_VAL_EXPLICIT((Obj*)newRangeIterator(range.start, range.stop, range.step));
  return UTRUE;
}

/* We still need this because the 'iter/next' API cannot support FastRangeIterator */
static CFunction funcFastRangeIter = { implFastRangeIter, "__iter__" };

void initFastRangeClass(void) {
  CFunction *methods[] = {
    &funcFastRangeIter,
    NULL,
  };
  newBuiltinClass(
    "FastRange",
    &vm.fastRangeClass,
    TYPE_PATTERN_ANY,
    methods,
    NULL);
}

static ubool implRangeIter(i16 argCount, Value *args, Value *out) {
  ObjRange *range = AS_RANGE(args[-1]);
  *out = OBJ_VAL_EXPLICIT((Obj*)newRangeIterator(range->start, range->stop, range->step));
  return UTRUE;
}

static CFunction funcRangeIter = { implRangeIter, "__iter__" };

static ubool implRangeRepr(i16 argCount, Value *args, Value *out) {
  ObjRange *range = AS_RANGE(args[-1]);
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
  return UTRUE;
}

static CFunction funcRangeRepr = { implRangeRepr, "__repr__" };

void initRangeClass(void) {
  CFunction *methods[] = {
    &funcRangeIter,
    &funcRangeRepr,
    NULL,
  };
  newNativeClass(NULL, &descriptorRange, methods, NULL);
}

static ubool implRangeIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjRangeIterator *iter = AS_RANGE_ITERATOR(args[-1]);
  if (iter->step > 0) {
    if (iter->next >= iter->stop) {
      *out = STOP_ITERATION_VAL();
      return UTRUE;
    }
  } else if (iter->next <= iter->stop) {
    *out = STOP_ITERATION_VAL();
    return UTRUE;
  }
  *out = NUMBER_VAL(iter->next);
  iter->next += iter->step;
  return UTRUE;
}

static CFunction funcRangeIteratorCall = { implRangeIteratorCall, "__call__" };

void initRangeIteratorClass(void) {
  CFunction *methods[] = {
    &funcRangeIteratorCall,
    NULL,
  };
  newNativeClass(NULL, &descriptorRangeIterator, methods, NULL);
}

NativeObjectDescriptor descriptorRange = {
  nopBlacken, nopFree,
  sizeof(ObjRange), "Range",
};

NativeObjectDescriptor descriptorRangeIterator = {
  nopBlacken, nopFree,
  sizeof(ObjRangeIterator), "RangeIterator",
};

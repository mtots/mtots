#ifndef mtots_class_range_h
#define mtots_class_range_h

#include "mtots_object.h"

#define AS_RANGE(value) ((ObjRange*)AS_OBJ(value))
#define IS_RANGE(value) (getNativeObjectDescriptor(value) == &descriptorRange)

#define AS_RANGE_ITERATOR(value) ((ObjRangeIterator*)AS_OBJ(value))
#define IS_RANGE_ITERATOR(value) (getNativeObjectDescriptor(value) == &descriptorRangeIterator)

typedef struct ObjRange {
  ObjNative obj;
  double start;
  double stop;
  double step;
} ObjRange;

typedef struct ObjRangeIterator {
  ObjNative obj;
  double next;
  double stop;
  double step;
} ObjRangeIterator;

void initFastRangeClass(void);
void initRangeClass(void);
void initRangeIteratorClass(void);

ObjRange *newRange(double start, double stop, double step);
ObjRangeIterator *newRangeIterator(double start, double stop, double step);

extern NativeObjectDescriptor descriptorRange;
extern NativeObjectDescriptor descriptorRangeIterator;

#endif/*mtots_class_range_h*/

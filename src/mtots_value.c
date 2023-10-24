#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_object.h"

size_t asSize(Value value) {
  double x = asNumber(value);

  /* There are actually some size_t values that cannot fit into
   * a double in a 64-bit platform. However, when specifying
   * size, this should be pretty rare since in a double we
   * still have over 50-bits, and so can address over
   * a petabyte */
  if (x != (double)(size_t)x) {
    panic("Expected size, but got a %f", x);
  }

  return (size_t)x;
}

ptrdiff_t asPtrdiff(Value value) {
  double x = asNumber(value);

  /* There are actually some ptrdiff_t values that cannot fit into
   * a double in a 64-bit platform. However, when specifying
   * size, this should be pretty rare since in a double we
   * still have over 50-bits, and so can address over
   * a petabyte */
  if (x != (double)(ptrdiff_t)x) {
    panic("Expected size, but got a %f", x);
  }

  return (ptrdiff_t)x;
}

int asInt(Value value) {
  double x = asNumber(value);
  if (x < (double)INT_MIN) {
    int v = INT_MIN;
    double vd = (double)v;
    panic("Expected int, but value is less than INT_MIN (%f, %f, %d)", x, -2147483648.0, (x < vd));
  }
  if (x > (double)INT_MAX) {
    panic("Expected int, but value is greater than INT_MAX (%f)", x);
  }
  return (int)x;
}

float asFloat(Value value) {
  return (float)asNumber(value);
}

u32 asU32Bits(Value value) {
  double x = asNumber(value);
  if (x < 0) {
    /* In this case, we may be generous and allow interpretation
     * to i32 first, and do a bitwise reinterpret */
    if (x < (double)I32_MIN) {
      panic("Expected u32bits, but value is less than I32_MIN (%f)", x);
    }
    return (u32)(i32)x;
  }
  if (x > U32_MAX) {
    panic("Expected u32bits, but value is greater than U32_MAX (%f)", x);
  }
  return (u32)x;
}

u32 asU32(Value value) {
  double x = asNumber(value);
  if (x < 0) {
    panic("Expected u32, but value is less than zero (%f)", x);
  }
  if (x > U32_MAX) {
    panic("Expected u32, but value is greater than U32_MAX (%f)", x);
  }
  return (u32)x;
}

i32 asI32(Value value) {
  double x = asNumber(value);
  if (x < (double)I32_MIN) {
    i32 v = I32_MIN;
    double vd = (double)v;
    panic("Expected i32, but value is less than I32_MIN (%f, %f, %d)", x, -2147483648.0, (x < vd));
  }
  if (x > (double)I32_MAX) {
    panic("Expected i32, but value is greater than I32_MAX (%f)", x);
  }
  return (i32)x;
}

u16 asU16(Value value) {
  double x = asNumber(value);
  if (x < 0) {
    panic("Expected u16, but value is less than zero (%f)", x);
  }
  if (x > U16_MAX) {
    panic("Expected u16, but value is greater than U16_MAX (%f)", x);
  }
  return (u16)x;
}

u8 asU8(Value value) {
  double x = asNumber(value);
  if (x < 0) {
    panic("Expected u8, but value is less than zero (%f)", x);
  }
  if (x > U8_MAX) {
    panic("Expected u8, but value is greater than U8_MAX (%f)", x);
  }
  return (u8)x;
}

size_t asIndex(Value value, size_t length) {
  double x = asNumber(value);
  if (x < 0) {
    x += length;
  }
  if (x < 0 || x >= length) {
    panic("Index out of bounds (i=%f, length=%lu)", x, length);
  }
  return (size_t)x;
}

size_t asIndexLower(Value value, size_t length) {
  double x = asNumber(value);
  if (x < 0) {
    x += length;
  }
  if (x < 0) {
    x = 0;
  } else if (x > length) {
    x = length;
  }
  return (size_t)x;
}

size_t asIndexUpper(Value value, size_t length) {
  double x = asNumber(value);
  if (x < 0) {
    x += length;
  }
  if (x < 0) {
    x = 0;
  } else if (x > length) {
    x = length;
  }
  return (size_t)x;
}

ubool asBool(Value value) {
  if (!isBool(value)) {
    panic("Expected Bool but got %s", getKindName(value));
  }
  return value.as.boolean;
}
double asNumber(Value value) {
  if (!isNumber(value)) {
    panic("Expected Number but got %s", getKindName(value));
  }
  return value.as.number;
}
String *asString(Value value) {
  if (!isString(value)) {
    panic("Expected String but got %s", getKindName(value));
  }
  return value.as.string;
}
CFunction *asCFunction(Value value) {
  if (!isCFunction(value)) {
    panic("Expected CFunction but got %s", getKindName(value));
  }
  return value.as.cfunction;
}
Range asRange(Value value) {
  Range range;
  if (!isRange(value)) {
    panic("Expected Range but got %s", getKindName(value));
  }
  range.start = value.extra.integer;
  range.stop = value.as.range.stop;
  range.step = value.as.range.step;
  return range;
}
RangeIterator asRangeIterator(Value value) {
  RangeIterator rangeIter;
  if (!isRangeIterator(value)) {
    panic("Expected RangeIterator but got %s", getKindName(value));
  }
  rangeIter.current = value.extra.integer;
  rangeIter.stop = value.as.range.stop;
  rangeIter.step = value.as.range.step;
  return rangeIter;
}
Vector asVector(Value value) {
  Vector vector;
  if (!isVector(value)) {
    panic("Expected Vector but got %s", getKindName(value));
  }
  vector.x = value.extra.floatingPoint;
  vector.y = value.as.vector.y;
  vector.z = value.as.vector.z;
  return vector;
}
Obj *asObj(Value value) {
  if (!isObj(value)) {
    panic("Expected Obj but got %s", getKindName(value));
  }
  return value.as.obj;
}

Value valNil(void) {
  Value v = {VAL_NIL};
  return v;
}
Value valBool(ubool value) {
  Value v = {VAL_BOOL};
  v.as.boolean = value != 0;
  return v;
}
Value valNumber(double value) {
  Value v = {VAL_NUMBER};
  v.as.number = value;
  return v;
}
Value valString(String *string) {
  Value v = {VAL_STRING};
  v.as.string = string;
  return v;
}
Value valCFunction(CFunction *func) {
  Value v = {VAL_CFUNCTION};
  v.as.cfunction = func;
  return v;
}
Value valSentinel(Sentinel sentinel) {
  Value v = {VAL_SENTINEL};
  v.as.sentinel = sentinel;
  return v;
}
Value valRange(Range range) {
  Value v = {VAL_RANGE};
  v.extra.integer = range.start;
  v.as.range.stop = range.stop;
  v.as.range.step = range.step;
  return v;
}
Value valRangeIterator(RangeIterator rangeIterator) {
  Value v = {VAL_RANGE_ITERATOR};
  v.extra.integer = rangeIterator.current;
  v.as.range.stop = rangeIterator.stop;
  v.as.range.step = rangeIterator.step;
  return v;
}
Value valVector(Vector vector) {
  Value v = {VAL_VECTOR};
  v.extra.floatingPoint = vector.x;
  v.as.vector.y = vector.y;
  v.as.vector.z = vector.z;
  return v;
}
Value valObjExplicit(Obj *object) {
  Value v = {VAL_OBJ};
  v.as.obj = object;
  return v;
}

Vector newVector(float x, float y, float z) {
  Vector vector;
  vector.x = x;
  vector.y = y;
  vector.z = z;
  return vector;
}

void fieldNotFoundError(Value owner, const char *fieldName) {
  runtimeError("Field '%s' not found on %s", fieldName, getKindName(owner));
}

void initValueArray(ValueArray *array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(
        Value, array->values, oldCapacity, array->capacity);
  }

  array->values[array->count++] = value;
}

void freeValueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

/* Returns a human readable string describing the 'kind' of the given value.
 * For non-object values, a string describing its value type is returned.
 * For object values, a string describing its object type is returned.
 */
const char *getKindName(Value value) {
  switch (value.type) {
    case VAL_NIL:
      return "Nil";
    case VAL_BOOL:
      return "Bool";
    case VAL_NUMBER:
      return "Number";
    case VAL_STRING:
      return "String";
    case VAL_CFUNCTION:
      return "Cfunction";
    case VAL_SENTINEL:
      return "Sentinel";
    case VAL_RANGE:
      return "Range";
    case VAL_RANGE_ITERATOR:
      return "RangeIterator";
    case VAL_VECTOR:
      return "Vector";
    case VAL_OBJ:
      switch (value.as.obj->type) {
        case OBJ_CLASS:
          return "Class";
        case OBJ_CLOSURE:
          return "Closure";
        case OBJ_THUNK:
          return "Thunk";
        case OBJ_INSTANCE:
          return "Instance";
        case OBJ_BUFFER:
          return "Buffer";
        case OBJ_LIST:
          return "List";
        case OBJ_FROZEN_LIST:
          return "FrozenList";
        case OBJ_DICT:
          return "Dict";
        case OBJ_FROZEN_DICT:
          return "FrozenDict";
        case OBJ_NATIVE:
          return AS_NATIVE_UNSAFE(value)->descriptor->name;
        case OBJ_UPVALUE:
          return "Upvalue";
      }
      return "<unrecognized-object>";
  }
  return "<unrecognized-value>";
}

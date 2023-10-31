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

PointerType asPointerType(Value value) {
  u16 number = asU16(value);
  if (number > POINTER_TYPE_DOUBLE) {
    panic("Expected PointerType but got value out of range (%d)", (int)number);
  }
  return (PointerType)number;
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
TypedPointer asPointer(Value value) {
  TypedPointer pointer;
  if (!isPointer(value)) {
    panic("Expected Pointer but got %s", getKindName(value));
  }
  pointer.metadata = value.extra.tpm;
  if (pointer.metadata.isConst) {
    pointer.as.constVoidPointer = value.as.constVoidPointer;
  } else {
    pointer.as.voidPointer = value.as.voidPointer;
  }
  return pointer;
}
FileDescriptor asFileDescriptor(Value value) {
  if (!isFileDescriptor(value)) {
    panic("Expected FileDescriptor but got %s", getKindName(value));
  }
  return value.as.fileDescriptor;
}
Obj *asObj(Value value) {
  if (!isObj(value)) {
    panic("Expected Obj but got %s", getKindName(value));
  }
  return value.as.obj;
}

void *asVoidPointer(Value value) {
  TypedPointer pointer = asPointer(value);
  if (pointer.metadata.isConst) {
    panic("Expected void pointer but got %s%s",
          pointer.metadata.isConst ? "const " : "",
          getPointerTypeName(pointer.metadata.type));
  }
  return pointer.as.voidPointer;
}
const void *asConstVoidPointer(Value value) {
  TypedPointer pointer = asPointer(value);
  return pointer.metadata.isConst ? pointer.as.constVoidPointer : pointer.as.voidPointer;
}
int *asIntPointer(Value value) {
  TypedPointer pointer = asPointer(value);
  if (pointer.metadata.isConst || pointer.metadata.type != POINTER_TYPE_INT) {
    panic("Expected int pointer but got %s%s",
          pointer.metadata.isConst ? "const " : "",
          getPointerTypeName(pointer.metadata.type));
  }
  return (int *)pointer.as.voidPointer;
}
u8 *asU8Pointer(Value value) {
  TypedPointer pointer = asPointer(value);
  if (pointer.metadata.isConst || pointer.metadata.type != POINTER_TYPE_U8) {
    panic("Expected u8 pointer but got %s%s",
          pointer.metadata.isConst ? "const " : "",
          getPointerTypeName(pointer.metadata.type));
  }
  return (u8 *)pointer.as.voidPointer;
}
u16 *asU16Pointer(Value value) {
  TypedPointer pointer = asPointer(value);
  if (pointer.metadata.isConst || pointer.metadata.type != POINTER_TYPE_U16) {
    panic("Expected u16 pointer but got %s%s",
          pointer.metadata.isConst ? "const " : "",
          getPointerTypeName(pointer.metadata.type));
  }
  return (u16 *)pointer.as.voidPointer;
}
u32 *asU32Pointer(Value value) {
  TypedPointer pointer = asPointer(value);
  if (pointer.metadata.isConst || pointer.metadata.type != POINTER_TYPE_U32) {
    panic("Expected u32 pointer but got %s%s",
          pointer.metadata.isConst ? "const " : "",
          getPointerTypeName(pointer.metadata.type));
  }
  return (u32 *)pointer.as.voidPointer;
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
Value valPointer(TypedPointer pointer) {
  Value v = {VAL_POINTER};
  v.extra.tpm = pointer.metadata;
  if (pointer.metadata.isConst) {
    v.as.constVoidPointer = pointer.as.constVoidPointer;
  } else {
    v.as.voidPointer = pointer.as.voidPointer;
  }
  return v;
}
Value valFileDescriptor(FileDescriptor fd) {
  Value v = {VAL_FILE_DESCRIPTOR};
  v.as.fileDescriptor = fd;
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

TypedPointer newConstTypedPointer(const void *pointer, PointerType type) {
  TypedPointer ret;
  ret.metadata.isConst = UTRUE;
  ret.metadata.type = type;
  ret.as.constVoidPointer = pointer;
  return ret;
}

TypedPointer newTypedPointer(void *pointer, PointerType type) {
  TypedPointer ret;
  ret.metadata.isConst = UFALSE;
  ret.metadata.type = type;
  ret.as.voidPointer = pointer;
  return ret;
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
    case VAL_POINTER:
      return "Pointer";
    case VAL_FILE_DESCRIPTOR:
      return "FileDescriptor";
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

const char *getPointerTypeName(PointerType type) {
  switch (type) {
    case POINTER_TYPE_VOID:
      return "POINTER_TYPE_VOID";
    case POINTER_TYPE_CHAR:
      return "POINTER_TYPE_CHAR";
    case POINTER_TYPE_SHORT:
      return "POINTER_TYPE_SHORT";
    case POINTER_TYPE_INT:
      return "POINTER_TYPE_INT";
    case POINTER_TYPE_LONG:
      return "POINTER_TYPE_LONG";
    case POINTER_TYPE_UNSIGNED_SHORT:
      return "POINTER_TYPE_UNSIGNED_SHORT";
    case POINTER_TYPE_UNSIGNED_INT:
      return "POINTER_TYPE_UNSIGNED_INT";
    case POINTER_TYPE_UNSIGNED_LONG:
      return "POINTER_TYPE_UNSIGNED_LONG";
    case POINTER_TYPE_U8:
      return "POINTER_TYPE_U8";
    case POINTER_TYPE_U16:
      return "POINTER_TYPE_U16";
    case POINTER_TYPE_U32:
      return "POINTER_TYPE_U32";
    case POINTER_TYPE_U64:
      return "POINTER_TYPE_U64";
    case POINTER_TYPE_I8:
      return "POINTER_TYPE_I8";
    case POINTER_TYPE_I16:
      return "POINTER_TYPE_I16";
    case POINTER_TYPE_I32:
      return "POINTER_TYPE_I32";
    case POINTER_TYPE_I64:
      return "POINTER_TYPE_I64";
    case POINTER_TYPE_SIZE_T:
      return "POINTER_TYPE_SIZE_T";
    case POINTER_TYPE_PTRDIFF_T:
      return "POINTER_TYPE_PTRDIFF_T";
    case POINTER_TYPE_FLOAT:
      return "POINTER_TYPE_FLOAT";
    case POINTER_TYPE_DOUBLE:
      return "POINTER_TYPE_DOUBLE";
  }
  return "Unknown-Pointer-Type";
}

size_t getPointerItemSize(PointerType type) {
  switch (type) {
    case POINTER_TYPE_VOID:
      return 0;
    case POINTER_TYPE_CHAR:
      return sizeof(char);
    case POINTER_TYPE_SHORT:
      return sizeof(short);
    case POINTER_TYPE_INT:
      return sizeof(int);
    case POINTER_TYPE_LONG:
      return sizeof(long);
    case POINTER_TYPE_UNSIGNED_SHORT:
      return sizeof(unsigned short);
    case POINTER_TYPE_UNSIGNED_INT:
      return sizeof(int);
    case POINTER_TYPE_UNSIGNED_LONG:
      return sizeof(long);
    case POINTER_TYPE_U8:
      return sizeof(u8);
    case POINTER_TYPE_U16:
      return sizeof(u16);
    case POINTER_TYPE_U32:
      return sizeof(u32);
    case POINTER_TYPE_U64:
      return sizeof(u64);
    case POINTER_TYPE_I8:
      return sizeof(i8);
    case POINTER_TYPE_I16:
      return sizeof(i16);
    case POINTER_TYPE_I32:
      return sizeof(i32);
    case POINTER_TYPE_I64:
      return sizeof(i64);
    case POINTER_TYPE_SIZE_T:
      return sizeof(size_t);
    case POINTER_TYPE_PTRDIFF_T:
      return sizeof(ptrdiff_t);
    case POINTER_TYPE_FLOAT:
      return sizeof(float);
    case POINTER_TYPE_DOUBLE:
      return sizeof(double);
  }
  return 0;
}

static const void *getConstPointer(TypedPointer ptr) {
  return ptr.metadata.isConst ? ptr.as.constVoidPointer : ptr.as.voidPointer;
}

static void *getNonConstPointer(TypedPointer ptr) {
  if (ptr.metadata.isConst) {
    panic("Required non-const pointer but got const pointer");
  }
  return ptr.as.voidPointer;
}

double derefTypedPointer(TypedPointer ptr, ptrdiff_t offset) {
  switch (ptr.metadata.type) {
    case POINTER_TYPE_VOID:
      panic("Cannot dereference a void pointer (derefTypedPointer)");
    case POINTER_TYPE_CHAR:
      return *((const char *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_SHORT:
      return *((const short *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_INT:
      return *((const int *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_LONG:
      return *((const long *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_UNSIGNED_SHORT:
      return *((const unsigned short *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_UNSIGNED_INT:
      return *((const unsigned int *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_UNSIGNED_LONG:
      return *((const unsigned long *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U8:
      return *((const u8 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U16:
      return *((const u16 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U32:
      return *((const u32 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U64:
      return *((const u64 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I8:
      return *((const i8 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I16:
      return *((const i16 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I32:
      return *((const i32 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I64:
      return *((const i64 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_SIZE_T:
      return *((const size_t *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_PTRDIFF_T:
      return *((const ptrdiff_t *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_FLOAT:
      return *((const float *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_DOUBLE:
      return *((const double *)getConstPointer(ptr) + offset);
  }
  panic("Invalid pointer type %d (derefTypedPointer)", ptr.metadata.type);
}

void assignToTypedPointer(TypedPointer ptr, ptrdiff_t offset, double value) {
  switch (ptr.metadata.type) {
    case POINTER_TYPE_VOID:
      panic("Cannot dereference a void pointer (assignToTypedPointer)");
    case POINTER_TYPE_CHAR:
      *((char *)getNonConstPointer(ptr) + offset) = (char)value;
      return;
    case POINTER_TYPE_SHORT:
      *((short *)getNonConstPointer(ptr) + offset) = (short)value;
      return;
    case POINTER_TYPE_INT:
      *((int *)getNonConstPointer(ptr) + offset) = (int)value;
      return;
    case POINTER_TYPE_LONG:
      *((long *)getNonConstPointer(ptr) + offset) = (long)value;
      return;
    case POINTER_TYPE_UNSIGNED_SHORT:
      *((unsigned short *)getNonConstPointer(ptr) + offset) = (unsigned short)value;
      return;
    case POINTER_TYPE_UNSIGNED_INT:
      *((unsigned int *)getNonConstPointer(ptr) + offset) = (unsigned int)value;
      return;
    case POINTER_TYPE_UNSIGNED_LONG:
      *((unsigned long *)getNonConstPointer(ptr) + offset) = (unsigned long)value;
      return;
    case POINTER_TYPE_U8:
      *((u8 *)getNonConstPointer(ptr) + offset) = (u8)value;
      return;
    case POINTER_TYPE_U16:
      *((u16 *)getNonConstPointer(ptr) + offset) = (u16)value;
      return;
    case POINTER_TYPE_U32:
      *((u32 *)getNonConstPointer(ptr) + offset) = (u32)value;
      return;
    case POINTER_TYPE_U64:
      *((u64 *)getNonConstPointer(ptr) + offset) = (u64)value;
      return;
    case POINTER_TYPE_I8:
      *((i8 *)getNonConstPointer(ptr) + offset) = (i8)value;
      return;
    case POINTER_TYPE_I16:
      *((i16 *)getNonConstPointer(ptr) + offset) = (i16)value;
      return;
    case POINTER_TYPE_I32:
      *((i32 *)getNonConstPointer(ptr) + offset) = (i32)value;
      return;
    case POINTER_TYPE_I64:
      *((i64 *)getNonConstPointer(ptr) + offset) = (i64)value;
      return;
    case POINTER_TYPE_SIZE_T:
      *((size_t *)getNonConstPointer(ptr) + offset) = (size_t)value;
      return;
    case POINTER_TYPE_PTRDIFF_T:
      *((ptrdiff_t *)getNonConstPointer(ptr) + offset) = (ptrdiff_t)value;
      return;
    case POINTER_TYPE_FLOAT:
      *((float *)getNonConstPointer(ptr) + offset) = (float)value;
      return;
    case POINTER_TYPE_DOUBLE:
      *((double *)getNonConstPointer(ptr) + offset) = (double)value;
      return;
  }
  panic("Invalid pointer type %d (assignToTypedPointer)", ptr.metadata.type);
}

TypedPointer addToTypedPointer(TypedPointer ptr, ptrdiff_t offset) {
  TypedPointer ret = ptr;
  size_t itemSize = getPointerItemSize(ptr.metadata.type);
  ptrdiff_t byteOffset = offset * itemSize;
  if (itemSize == 0) {
    panic("Cannot perform addToTypedPointer on %s",
          getPointerTypeName(ptr.metadata.type));
  }
  if (ret.metadata.isConst) {
    ret.as.constVoidPointer = ((const u8 *)ptr.as.constVoidPointer) + byteOffset;
  } else {
    ret.as.voidPointer = ((u8 *)ptr.as.voidPointer) + byteOffset;
  }
  return ret;
}

ptrdiff_t subtractFromTypedPointer(TypedPointer p1, TypedPointer p2) {
  size_t itemSize;
  if (p1.metadata.type != p2.metadata.type) {
    panic(
        "Pointer types must match when subtracting pointers "
        "(subtractFromTypedPointer)");
  }
  itemSize = getPointerItemSize(p1.metadata.type);
  if (itemSize == 0) {
    panic("Cannot perform subtractFromTypedPointer on %s",
          getPointerTypeName(p1.metadata.type));
  }
  return ((const u8 *)getConstPointer(p1) - (const u8 *)getConstPointer(p2)) / itemSize;
}

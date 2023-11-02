#ifndef mtots_value_h
#define mtots_value_h

#include "mtots_util.h"

typedef struct CFunction CFunction;
typedef struct Obj Obj;

typedef enum Sentinel {
  SentinelStopIteration,
  SentinelEmptyKey /* Used internally in Map */
} Sentinel;

typedef enum ValueType {
  VAL_NIL,
  VAL_BOOL,
  VAL_NUMBER,
  VAL_STRING,
  VAL_CFUNCTION,
  VAL_SENTINEL,

  /* optimization values */
  VAL_RANGE,
  VAL_RANGE_ITERATOR,

  /* other useful types */
  VAL_VECTOR,

  /* dangerous, but useful for interfacing with C */
  VAL_POINTER,
  VAL_FILE_DESCRIPTOR,

  VAL_OBJ
} ValueType;

typedef struct Range {
  i32 start;
  i32 stop;
  i32 step;
} Range;

typedef struct RangeIterator {
  i32 current;
  i32 stop;
  i32 step;
} RangeIterator;

typedef struct RangePartial {
  i32 stop;
  i32 step;
} RangePartial;

typedef struct Vector {
  float x;
  float y;
  float z;
} Vector;

typedef struct VectorPartial {
  float y;
  float z;
} VectorPartial;

typedef enum PointerType {
  POINTER_TYPE_VOID,

  POINTER_TYPE_CHAR,
  POINTER_TYPE_SHORT,
  POINTER_TYPE_INT,
  POINTER_TYPE_LONG,

  POINTER_TYPE_UNSIGNED_SHORT,
  POINTER_TYPE_UNSIGNED_INT,
  POINTER_TYPE_UNSIGNED_LONG,

  POINTER_TYPE_U8,
  POINTER_TYPE_U16,
  POINTER_TYPE_U32,
  POINTER_TYPE_U64,

  POINTER_TYPE_I8,
  POINTER_TYPE_I16,
  POINTER_TYPE_I32,
  POINTER_TYPE_I64,

  POINTER_TYPE_SIZE_T,
  POINTER_TYPE_PTRDIFF_T,

  POINTER_TYPE_FLOAT,
  POINTER_TYPE_DOUBLE
} PointerType;

/* Must fit in 4-bytes */
typedef struct TypedPointerMetadata {
  u16 type; /* PointerType */
  ubool isConst;
} TypedPointerMetadata;

typedef struct TypedPointer {
  TypedPointerMetadata metadata;
  union {
    void *voidPointer;
    const void *constVoidPointer;
  } as;
} TypedPointer;

/* Value struct should be 16-bytes on all supported platforms */
typedef struct Value {
  ValueType type; /* 4-bytes */

  union {
    i32 integer;              /* for Range and RangeIterator */
    float floatingPoint;      /* for Vector */
    TypedPointerMetadata tpm; /* for TypedPointer */
  } extra;                    /* 4-bytes */

  /*
   * All members of this union are either:
   *   * 4-bytes or less (enums, ubool, and pointers on 32-bit systems), or
   *   * 8-bytes (double, fastRange, and pointers on 64-bit systems)
   */
  union {
    ubool boolean;
    double number;
    String *string;
    CFunction *cfunction;
    Sentinel sentinel;
    RangePartial range;
    VectorPartial vector;
    void *voidPointer;            /* for TypedPointer */
    const void *constVoidPointer; /* for TypedPointer */
    FileDescriptor fileDescriptor;
    Obj *obj;
  } as; /* 8-bytes */
} Value;

struct CFunction {
  Status (*body)(i16 argCount, Value *args, Value *out);
  const char *name;
  i16 arity;
  i16 maxArity;
  const char **parameterNames;
  String **parameterNameStrings;
};

typedef struct ValueArray {
  size_t capacity;
  size_t count;
  Value *values;
} ValueArray;

#define isNil(value) ((value).type == VAL_NIL)
#define isBool(value) ((value).type == VAL_BOOL)
#define isNumber(value) ((value).type == VAL_NUMBER)
#define isString(value) ((value).type == VAL_STRING)
#define isCFunction(value) ((value).type == VAL_CFUNCTION)
#define isSentinel(value) ((value).type == VAL_SENTINEL)
#define isRange(value) ((value).type == VAL_RANGE)
#define isRangeIterator(value) ((value).type == VAL_RANGE_ITERATOR)
#define isVector(value) ((value).type == VAL_VECTOR)
#define isPointer(value) ((value).type == VAL_POINTER)
#define isFileDescriptor(value) ((value).type == VAL_FILE_DESCRIPTOR)
#define isObj(value) ((value).type == VAL_OBJ)
#define AS_OBJ_UNSAFE(value) ((value).as.obj)

size_t asSize(Value value);
ptrdiff_t asPtrdiff(Value value);
int asInt(Value value);
unsigned asUnsigned(Value value);
float asFloat(Value value);
u32 asU32Bits(Value value);
u32 asU32(Value value);
i32 asI32(Value value);
u16 asU16(Value value);
u8 asU8(Value value);
size_t asIndex(Value value, size_t length);
size_t asIndexLower(Value value, size_t length);
size_t asIndexUpper(Value value, size_t length);
PointerType asPointerType(Value value);

ubool asBool(Value value);
double asNumber(Value value);
String *asString(Value value);
CFunction *asCFunction(Value value);
Range asRange(Value value);
RangeIterator asRangeIterator(Value value);
Vector asVector(Value value);
TypedPointer asPointer(Value value);
FileDescriptor asFileDescriptor(Value value);
Obj *asObj(Value value);

void *asCheckedPointer(Value value, PointerType type);
const void *asCheckedConstPointer(Value value, PointerType type);
void *asVoidPointer(Value value);
const void *asConstVoidPointer(Value value);

#define asCharPointer(v) ((char *)asCheckedPointer((v), POINTER_TYPE_CHAR))
#define asShortPointer(v) ((short *)asCheckedPointer((v), POINTER_TYPE_SHORT))
#define asIntPointer(v) ((int *)asCheckedPointer((v), POINTER_TYPE_INT))
#define asLongPointer(v) ((long *)asCheckedPointer((v), POINTER_TYPE_LONG))
#define asUnsignedShortPointer(v) ((unsigned short *)asCheckedPointer((v), POINTER_TYPE_UNSIGNED_SHORT))
#define asUnsignedIntPointer(v) ((unsigned int *)asCheckedPointer((v), POINTER_TYPE_UNSIGNED_INT))
#define asUnsignedLongPointer(v) ((unsigned long *)asCheckedPointer((v), POINTER_TYPE_UNSIGNED_LONG))
#define asU8Pointer(v) ((u8 *)asCheckedPointer((v), POINTER_TYPE_U8))
#define asU16Pointer(v) ((u16 *)asCheckedPointer((v), POINTER_TYPE_U16))
#define asU32Pointer(v) ((u32 *)asCheckedPointer((v), POINTER_TYPE_U32))
#define asU64Pointer(v) ((u64 *)asCheckedPointer((v), POINTER_TYPE_U64))
#define asI8Pointer(v) ((i8 *)asCheckedPointer((v), POINTER_TYPE_I8))
#define asI16Pointer(v) ((i16 *)asCheckedPointer((v), POINTER_TYPE_I16))
#define asI32Pointer(v) ((i32 *)asCheckedPointer((v), POINTER_TYPE_I32))
#define asI64Pointer(v) ((i64 *)asCheckedPointer((v), POINTER_TYPE_I64))
#define asSizeTPointer(v) ((size_t *)asCheckedPointer((v), POINTER_TYPE_SIZE_T))
#define asPtrdiffTPointer(v) ((ptrdiff_t *)asCheckedPointer((v), POINTER_TYPE_PTRDIFF_T))
#define asFloatPointer(v) ((float *)asCheckedPointer((v), POINTER_TYPE_FLOAT))
#define asDoublePointer(v) ((double *)asCheckedPointer((v), POINTER_TYPE_DOUBLE))

#define asConstCharPointer(v) ((const char *)asCheckedConstPointer((v), POINTER_TYPE_CHAR))
#define asConstShortPointer(v) ((const short *)asCheckedConstPointer((v), POINTER_TYPE_SHORT))
#define asConstIntPointer(v) ((const int *)asCheckedConstPointer((v), POINTER_TYPE_INT))
#define asConstLongPointer(v) ((const long *)asCheckedConstPointer((v), POINTER_TYPE_LONG))
#define asConstUnsignedShortPointer(v) \
  ((const unsigned short *Const)asCheckedConstPointer((v), POINTER_TYPE_UNSIGNED_SHORT))
#define asConstUnsignedIntPointer(v) \
  ((const unsigned int *)asCheckedConstPointer((v), POINTER_TYPE_UNSIGNED_INT))
#define asConstUnsignedLongPointer(v) \
  ((const unsigned long *)asCheckedConstPointer((v), POINTER_TYPE_UNSIGNED_LONG))
#define asConstU8Pointer(v) ((const u8 *)asCheckedConstPointer((v), POINTER_TYPE_U8))
#define asConstU16Pointer(v) ((const u16 *)asCheckedConstPointer((v), POINTER_TYPE_U16))
#define asConstU32Pointer(v) ((const u32 *)asCheckedConstPointer((v), POINTER_TYPE_U32))
#define asConstU64Pointer(v) ((const u64 *)asCheckedConstPointer((v), POINTER_TYPE_U64))
#define asConstI8Pointer(v) ((const i8 *)asCheckedConstPointer((v), POINTER_TYPE_I8))
#define asConstI16Pointer(v) ((const i16 *)asCheckedConstPointer((v), POINTER_TYPE_I16))
#define asConstI32Pointer(v) ((const i32 *)asCheckedConstPointer((v), POINTER_TYPE_I32))
#define asConstI64Pointer(v) ((const i64 *)asCheckedConstPointer((v), POINTER_TYPE_I64))
#define asConstSizeTPointer(v) ((const size_t *)asCheckedConstPointer((v), POINTER_TYPE_SIZE_T))
#define asConstPtrdiffTPointer(v) \
  ((const ptrdiff_t *)asChConsteckedPointer((v), POINTER_TYPE_PTRDIFF_T))
#define asConstFloatPointer(v) ((const float *)asCheckedConstPointer((v), POINTER_TYPE_FLOAT))
#define asConstDoublePointer(v) ((const double *)asCheckedConstPointer((v), POINTER_TYPE_DOUBLE))

Value valNil(void);
Value valBool(ubool value);
Value valNumber(double value);
Value valString(String *string);
Value valCFunction(CFunction *func);
Value valSentinel(Sentinel sentinel);
Value valRange(Range range);
Value valRangeIterator(RangeIterator rangeIterator);
Value valPointer(TypedPointer pointer);
Value valFileDescriptor(FileDescriptor fd);
Value valVector(Vector vector);
Value valObjExplicit(Obj *object);

Vector newVector(float x, float y, float z);
TypedPointer newConstTypedPointer(const void *pointer, PointerType type);
TypedPointer newTypedPointer(void *pointer, PointerType type);

void fieldNotFoundError(Value owner, const char *fieldName);

#define isStopIteration(value) ( \
    isSentinel(value) &&         \
    ((value).as.sentinel == SentinelStopIteration))
#define isEmptyKey(value) ( \
    isSentinel(value) &&    \
    ((value).as.sentinel == SentinelEmptyKey))

#define valStopIteration() (valSentinel(SentinelStopIteration))
#define valEmptyKey() (valSentinel(SentinelEmptyKey))

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
const char *getKindName(Value value);

const char *getPointerTypeName(PointerType type);
size_t getPointerItemSize(PointerType type);
double derefTypedPointer(TypedPointer ptr, ptrdiff_t offset);
void assignToTypedPointer(TypedPointer ptr, ptrdiff_t offset, double value);
TypedPointer addToTypedPointer(TypedPointer ptr, ptrdiff_t offset);
ptrdiff_t subtractFromTypedPointer(TypedPointer p1, TypedPointer p2);

#endif /*mtots_value_h*/

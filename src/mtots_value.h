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

/* Value struct should be 16-bytes on all supported platforms */
typedef struct Value {
  ValueType type; /* 4-bytes */

  union {
    i32 integer; /* placeholder, for now */
  } extra;       /* 4-bytes */

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
#define isObj(value) ((value).type == VAL_OBJ)
#define AS_OBJ_UNSAFE(value) ((value).as.obj)

size_t asSize(Value value);
int asInt(Value value);
float asFloat(Value value);
u32 asU32Bits(Value value);
u32 asU32(Value value);
i32 asI32(Value value);
u8 asU8(Value value);
size_t asIndex(Value value, size_t length);
size_t asIndexLower(Value value, size_t length);
size_t asIndexUpper(Value value, size_t length);

ubool asBool(Value value);
double asNumber(Value value);
String *asString(Value value);
CFunction *asCFunction(Value value);
Range asRange(Value value);
RangeIterator asRangeIterator(Value value);
Obj *asObj(Value value);

Value valNil(void);
Value valBool(ubool value);
Value valNumber(double value);
Value valString(String *string);
Value valCFunction(CFunction *func);
Value valSentinel(Sentinel sentinel);
Value valRange(Range range);
Value valRangeIterator(RangeIterator rangeIterator);
Value valObjExplicit(Obj *object);

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

/* Just a convenience function to check that a Value is
 * equal to the given C-string */
ubool valueIsCString(Value value, const char *string);

#endif /*mtots_value_h*/

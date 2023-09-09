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
  VAL_OBJ
} ValueType;

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
    Obj *obj;
  } as; /* 8-bytes */
} Value;

struct CFunction {
  Status (*body)(i16 argCount, Value *args, Value *out);
  const char *name;
  i16 arity;
  i16 maxArity;
};

typedef struct ValueArray {
  size_t capacity;
  size_t count;
  Value *values;
} ValueArray;

#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_STRING(value) ((value).type == VAL_STRING)
#define IS_CFUNCTION(value) ((value).type == VAL_CFUNCTION)
#define IS_SENTINEL(value) ((value).type == VAL_SENTINEL)
#define IS_OBJ(value) ((value).type == VAL_OBJ)
#define AS_OBJ_UNSAFE(value) ((value).as.obj)

size_t asSize(Value value);
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
Obj *asObj(Value value);

Value NIL_VAL(void);
Value BOOL_VAL(ubool value);
Value NUMBER_VAL(double value);
Value STRING_VAL(String *string);
Value CFUNCTION_VAL(CFunction *func);
Value SENTINEL_VAL(Sentinel sentinel);
Value OBJ_VAL_EXPLICIT(Obj *object);

#define IS_STOP_ITERATION(value) ( \
    IS_SENTINEL(value) &&          \
    ((value).as.sentinel == SentinelStopIteration))
#define IS_EMPTY_KEY(value) ( \
    IS_SENTINEL(value) &&     \
    ((value).as.sentinel == SentinelEmptyKey))

#define STOP_ITERATION_VAL() (SENTINEL_VAL(SentinelStopIteration))
#define EMPTY_KEY_VAL() (SENTINEL_VAL(SentinelEmptyKey))

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
const char *getKindName(Value value);

/* Just a convenience function to check that a Value is
 * equal to the given C-string */
ubool valueIsCString(Value value, const char *string);

#endif /*mtots_value_h*/

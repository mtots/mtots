#ifndef mtots_value_h
#define mtots_value_h

#include "mtots_util.h"

typedef struct Builtin Builtin;
typedef struct CFunction CFunction;
typedef struct Obj Obj;

typedef enum TypePatternType {
  TYPE_PATTERN_ANY = 0,
  TYPE_PATTERN_STRING_OR_NIL,
  TYPE_PATTERN_STRING,
  TYPE_PATTERN_BUFFER_OR_NIL,
  TYPE_PATTERN_BUFFER,
  TYPE_PATTERN_BOOL,
  TYPE_PATTERN_NUMBER_OR_NIL,
  TYPE_PATTERN_NUMBER,
  TYPE_PATTERN_LIST_OR_NIL,
  TYPE_PATTERN_LIST,
  TYPE_PATTERN_LIST_NUMBER_OR_NIL,
  TYPE_PATTERN_LIST_NUMBER,
  TYPE_PATTERN_LIST_LIST_NUMBER, /* List of List of Number */
  TYPE_PATTERN_FROZEN_LIST,
  TYPE_PATTERN_DICT,
  TYPE_PATTERN_FROZEN_DICT,
  TYPE_PATTERN_CLASS,
  TYPE_PATTERN_NATIVE_OR_NIL,
  TYPE_PATTERN_NATIVE
} TypePatternType;

typedef struct TypePattern {
  TypePatternType type;
  void *nativeTypeDescriptor;
} TypePattern;

typedef enum Sentinel {
  SentinelStopIteration,
  SentinelEmptyKey        /* Used internally in Map */
} Sentinel;

typedef enum ValueType {
  VAL_NIL,
  VAL_BOOL,
  VAL_NUMBER,
  VAL_SYMBOL,
  VAL_STRING,
  VAL_BUILTIN,
  VAL_CFUNCTION,
  VAL_SENTINEL,
  VAL_OBJ
} ValueType;

/* Value struct should be 16-bytes on all supported platforms */
typedef struct Value {

  ValueType type; /* 4-bytes */

  union {
    i32 integer;       /* placeholder, for now */
  } extra; /* 4-bytes */

  /*
   * All members of this union are either:
   *   * 4-bytes or less (enums, ubool, and pointers on 32-bit systems), or
   *   * 8-bytes (double, fastRange, and pointers on 64-bit systems)
   */
  union {
    ubool boolean;
    double number;
    Symbol *symbol;
    String *string;
    Builtin *builtin;
    CFunction *cfunction;
    Sentinel sentinel;
    Obj *obj;
  } as; /* 8-bytes */
} Value;

struct Builtin {
  const char *name;
  Status (*body)(i16 argc, Value *argv, Value *out);
  i16 arity;
  i16 maxArity;
};

struct CFunction {
  ubool (*body)(i16 argCount, Value *args, Value *out);
  const char *name;
  i16 arity;
  i16 maxArity;
  TypePattern *argTypes;
  TypePattern receiverType;
};

typedef struct ValueArray {
  size_t capacity;
  size_t count;
  Value *values;
} ValueArray;

#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_SYMBOL(value) ((value).type == VAL_SYMBOL)
#define IS_STRING(value) ((value).type == VAL_STRING)
#define IS_BUILTIN(value) ((value).type == VAL_BUILTIN)
#define IS_CFUNCTION(value) ((value).type == VAL_CFUNCTION)
#define IS_SENTINEL(value) ((value).type == VAL_SENTINEL)
#define IS_FAST_RANGE(value) ((value).type == VAL_FAST_RANGE)
#define IS_FAST_RANGE_ITERATOR(value) ((value).type == VAL_FAST_RANGE_ITERATOR)
#define IS_FAST_LIST_ITERATOR(value) ((value).type == VAL_FAST_LIST_ITERATOR)
#define IS_COLOR(value) ((value).type == VAL_COLOR)
#define IS_VECTOR(value) ((value).type == VAL_VECTOR)
#define IS_RECT(value) ((value).type == VAL_RECT)
#define IS_OBJ(value) ((value).type == VAL_OBJ)
#define AS_OBJ(value) ((value).as.obj)
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_STRING(value) ((value).as.string)
#define AS_CSTRING(value) ((value).as.string->chars)
#define AS_CFUNCTION(value) ((value).as.cfunction)
#define AS_SENTINEL(value) ((value).as.sentinel)
#define AS_COLOR(value) ((value).as.color)

size_t AS_SIZE(Value value);
u32 AS_U32_BITS(Value value);
u32 AS_U32(Value value);
i32 AS_I32(Value value);
u16 AS_U16(Value value);
i16 AS_I16(Value value);
u8 AS_U8(Value value);
u8 AS_U8_CLAMP(Value value);
size_t AS_INDEX(Value value, size_t length);
size_t AS_INDEX_LOWER(Value value, size_t length);
size_t AS_INDEX_UPPER(Value value, size_t length);

ubool asBool(Value value);
double asNumber(Value value);
Symbol *asSymbol(Value value);
String *asString(Value value);
Builtin *asBuiltin(Value value);
CFunction *asCFunction(Value value);
Obj *asObj(Value value);

Value NIL_VAL(void);
Value BOOL_VAL(ubool value);
Value NUMBER_VAL(double value);
Value STRING_VAL(String *string);
Value BUILTIN_VAL(Builtin *builtin);
Value CFUNCTION_VAL(CFunction *func);
Value SENTINEL_VAL(Sentinel sentinel);
Value OBJ_VAL_EXPLICIT(Obj *object);

#define IS_STOP_ITERATION(value) ( \
  IS_SENTINEL(value) && \
  ((value).as.sentinel == SentinelStopIteration))
#define IS_EMPTY_KEY(value) ( \
  IS_SENTINEL(value) && \
  ((value).as.sentinel == SentinelEmptyKey))

#define STOP_ITERATION_VAL() (SENTINEL_VAL(SentinelStopIteration))
#define EMPTY_KEY_VAL() (SENTINEL_VAL(SentinelEmptyKey))

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value);
const char *getValueTypeName(ValueType type);
const char *getKindName(Value value);

ubool typePatternMatch(TypePattern pattern, Value value);
const char *getTypePatternName(TypePattern pattern);

/* Just a convenience function to check that a Value is
 * equal to the given C-string */
ubool valueIsCString(Value value, const char *string);

/* Common TypePattern arrays */

extern TypePattern argsNumbers[12];
extern TypePattern argsStrings[12];
extern TypePattern argsSetattr[2];

#endif/*mtots_value_h*/

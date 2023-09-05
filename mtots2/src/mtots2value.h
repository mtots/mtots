#ifndef mtots2value_h
#define mtots2value_h

#include "mtots1symbol.h"

typedef struct Object Object;
typedef struct Value Value;
typedef struct CFunction CFunction;
typedef struct Class Class; /** Mtots Class */
typedef struct String String;
typedef struct Map Map;

typedef enum ValueType {
  /* Sentinel is used for newSymbolal things (e.g. empty dict key) */
  VALUE_SENTINEL,

  /* Regular fixed size values */
  VALUE_NIL,
  VALUE_BOOL,
  VALUE_NUMBER,
  VALUE_SYMBOL,
  VALUE_CFUNCTION,
  VALUE_CLASS,

  /* Heap allocated value */
  VALUE_OBJECT
} ValueType;

struct CFunction {
  const char *name;
  i16 arity;
  i16 maxArity;
  Status (*body)(i16 argc, Value *argv, Value *out);
};

struct Value {
  ValueType type;
  union ValueAs {
    ubool boolean;
    double number;
    Symbol *symbol;
    CFunction *cfunction;
    Class *cls;
    Object *object;
  } as;
};

typedef struct CachedMethods {
  Value init;
  Value repr;
} CachedMethods;

/** Classes are never garbage collected. */
struct Class {
  const char *name;

  /** Size of the instance.
   * Only used for Native instances.
   * Non-Native values should have this value set to zero. */
  size_t size;

  /** If the type does not have a normal constructor,
   * this value will be NULL */
  CFunction *constructor;

  /** Builtin types will know how to destroy itself in
   * in `releaseValue`, and will have its destructor
   * value set to NULL */
  void (*destructor)(Object *);

  CFunction **nativeStaticMethods;
  CFunction **nativeInstanceMethods;
  Map *staticMethods;
  Map *instanceMethods;
  CachedMethods cachedMethods;
};

extern Class SENTINEL_CLASS;
extern Class NIL_CLASS;
extern Class BOOL_CLASS;
extern Class NUMBER_CLASS;
extern Class SYMBOL_CLASS;
extern Class CFUNCTION_CLASS;
extern Class CLASS_CLASS;

const char *getValueTypeName(ValueType type);
const char *getValueKindName(Value value);

/* memory management */

void retainValue(Value value);
void releaseValue(Value value);

/* value constructors */

Value sentinelValue(void);
Value nilValue(void);
Value boolValue(ubool x);
Value numberValue(double x);
Value symbolValue(Symbol *x);
Value cfunctionValue(CFunction *x);
Value classValue(Class *x);
Value objectValue(Object *x);

/* checked access functions */

#define isSentinel(v) ((v).type == VALUE_SENTINEL)
#define isNil(v) ((v).type == VALUE_NIL)
#define isBool(v) ((v).type == VALUE_BOOL)
#define isNumber(v) ((v).type == VALUE_NUMBER)
#define isSymbol(v) ((v).type == VALUE_SYMBOL)
#define isCFunction(v) ((v).type == VALUE_CFUNCTION)
#define isClass(v) ((v).type == VALUE_CLASS)
#define isObject(v) ((v).type == VALUE_OBJECT)

ubool asBool(Value value);
double asNumber(Value value);
Symbol *asSymbol(Value value);
CFunction *asCFunction(Value value);
Class *asClass(Value value);
Object *asObject(Value value);

/* operations on values */

Class *getClass(Value x);

/** Calls a function with the given arguments.
 * NOTE: The output value `out` must be manually
 * released after the call ends.
 * NOTE: If `function` is a method, `argv[-1]` must be a valid
 * receiver of the correct type. `argc` should NOT count the
 * receiver as an argument */
Status callValue(Value function, i16 argc, Value *argv, Value *out);

/** NOTE: The output value `out` must be released after the call.
 * NOTE: the owner/receiver should be provided as argv[-1] */
Status callMethod(Symbol *name, i16 argc, Value *argv, Value *out);

void reprValue(String *out, Value value);
void strValue(String *out, Value value);
void printValue(Value value);
ubool eqValue(Value a, Value b);
u32 hashValue(Value a);
void freezeValue(Value a);

/** Tests whether the given value is 'truthy' or not */
ubool testValue(Value a);
Value negateValue(Value a);
Value addValues(Value a, Value b);
Value subtractValues(Value a, Value b);
Value multiplyValues(Value a, Value b);
Value moduloValues(Value a, Value b);
Value divideValues(Value a, Value b);
Value floorDivideValues(Value a, Value b);

/** Creates a new class with the given name.
 * Classes created this way do not need to be 'finalized'.
 * (finalizing is for classes that are allocated
 * on static memory) */
Class *newClass(const char *name);

void classAddStaticMethod(Class *cls, Symbol *name, Value method);
void classAddInstanceMethod(Class *cls, Symbol *name, Value method);

/** Populates the method `Map`s with the native CFunctions */
void initStaticClass(Class *cls);

#endif /*mtots2value_h*/

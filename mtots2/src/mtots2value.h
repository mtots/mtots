#ifndef mtots2value_h
#define mtots2value_h

#include "mtots1symbol.h"

typedef struct Object Object;
typedef struct Value Value;
typedef struct CFunction CFunction;
typedef struct String String;

typedef enum ValueType {
  /* Sentinel is used for newSymbolal things (e.g. empty dict key) */
  VALUE_SENTINEL,

  /* Regular fixed size values */
  VALUE_NIL,
  VALUE_BOOL,
  VALUE_NUMBER,
  VALUE_SYMBOL,
  VALUE_CFUNCTION,

  /* Heap allocated value */
  VALUE_OBJECT
} ValueType;

struct CFunction {
  const char *name;
  Status (*body)(i16 argc, Value *argv, Value *out);
  i16 arity;
  i16 maxArity;
};

struct Value {
  ValueType type;
  union ValueAs {
    ubool boolean;
    double number;
    Symbol *symbol;
    CFunction *cfunction;
    Object *object;
  } as;
};

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
Value objectValue(Object *x);

/* checked access functions */

#define isSentinel(v) ((v).type == VALUE_SENTINEL)
#define isNil(v) ((v).type == VALUE_NIL)
#define isBool(v) ((v).type == VALUE_BOOL)
#define isNumber(v) ((v).type == VALUE_NUMBER)
#define isSymbol(v) ((v).type == VALUE_SYMBOL)
#define isCFunction(v) ((v).type == VALUE_CFUNCTION)
#define isObject(v) ((v).type == VALUE_OBJECT)

ubool asBool(Value value);
double asNumber(Value value);
Symbol *asSymbol(Value value);
CFunction *asCFunction(Value value);
Object *asObject(Value value);

/* operations on values */

void reprValue(String *out, Value value);
void printValue(Value value);
ubool eqValue(Value a, Value b);
u32 hashValue(Value a);

#endif /*mtots2value_h*/

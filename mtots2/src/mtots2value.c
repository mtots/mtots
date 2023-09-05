#include "mtots2value.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "mtots1err.h"
#include "mtots1symbol.h"
#include "mtots2list.h"
#include "mtots2map.h"
#include "mtots2native.h"
#include "mtots2string.h"

Class SENTINEL_CLASS = {
    "Sentinel", /* name */
    0,          /* size */
    NULL,       /* constructor */
    NULL,       /* desctructor */
};

Class NIL_CLASS = {
    "Nil", /* name */
    0,     /* size */
    NULL,  /* constructor */
    NULL,  /* desctructor */
};

Class BOOL_CLASS = {
    "Bool", /* name */
    0,      /* size */
    NULL,   /* constructor */
    NULL,   /* destructor */
};

Class NUMBER_CLASS = {
    "Number", /* name */
    0,        /* size */
    NULL,     /* constructor */
    NULL,     /* destructor */
};

Class SYMBOL_CLASS = {
    "Symbol", /* name */
    0,        /* size */
    NULL,     /* constructor */
    NULL,     /* destructor */
};

Class CFUNCTION_CLASS = {
    "CFunction", /* name */
    0,           /* size */
    NULL,        /* constructor */
    NULL,        /* destructor */
};

Class CLASS_CLASS = {
    "Class", /* name */
    0,       /* size */
    NULL,    /* constructor */
    NULL,    /* destructor */
};

const char *getValueTypeName(ValueType type) {
  switch (type) {
    case VALUE_SENTINEL:
      return "Sentinel";
    case VALUE_NIL:
      return "Nil";
    case VALUE_BOOL:
      return "Bool";
    case VALUE_NUMBER:
      return "Number";
    case VALUE_SYMBOL:
      return "Symbol";
    case VALUE_CFUNCTION:
      return "CFunction";
    case VALUE_CLASS:
      return "Class";
    case VALUE_OBJECT:
      return "Object";
  }
  return "INVALID_VALUE_TYPE";
}

const char *getValueKindName(Value value) {
  if (value.type == VALUE_OBJECT) {
    if (value.as.object->type == OBJECT_NATIVE) {
      return ((Native *)value.as.object)->cls->name;
    }
    return getObjectTypeName(value.as.object->type);
  }
  return getValueTypeName(value.type);
}

void retainValue(Value value) {
  if (value.type == VALUE_OBJECT) {
    retainObject(value.as.object);
  }
}

void releaseValue(Value value) {
  if (value.type == VALUE_OBJECT) {
    releaseObject(value.as.object);
  }
}

Value sentinelValue(void) {
  Value value;
  value.type = VALUE_SENTINEL;
  return value;
}

Value nilValue(void) {
  Value value;
  value.type = VALUE_NIL;
  return value;
}

Value boolValue(ubool x) {
  Value value;
  value.type = VALUE_BOOL;
  value.as.boolean = x;
  return value;
}

Value numberValue(double x) {
  Value value;
  value.type = VALUE_NUMBER;
  value.as.number = x;
  return value;
}

Value symbolValue(Symbol *x) {
  Value value;
  value.type = VALUE_SYMBOL;
  value.as.symbol = x;
  return value;
}

Value cfunctionValue(CFunction *x) {
  Value value;
  value.type = VALUE_CFUNCTION;
  value.as.cfunction = x;
  return value;
}

Value classValue(Class *x) {
  Value value;
  value.type = VALUE_CLASS;
  value.as.cls = x;
  return value;
}

Value objectValue(Object *x) {
  Value value;
  value.type = VALUE_OBJECT;
  value.as.object = x;
  return value;
}

ubool asBool(Value value) {
  if (!isBool(value)) {
    panic("Expected Bool but got %s", getValueKindName(value));
  }
  return value.as.boolean;
}

double asNumber(Value value) {
  if (!isNumber(value)) {
    panic("Expected Number but got %s", getValueKindName(value));
  }
  return value.as.number;
}

Symbol *asSymbol(Value value) {
  if (!isSymbol(value)) {
    panic("Expected Symbol but got %s", getValueKindName(value));
  }
  return value.as.symbol;
}

CFunction *asCFunction(Value value) {
  if (!isCFunction(value)) {
    panic("Expected CFunction but got %s", getValueKindName(value));
  }
  return value.as.cfunction;
}

Class *asClass(Value value) {
  if (!isClass(value)) {
    panic("Expected Class but got %s", getValueKindName(value));
  }
  return value.as.cls;
}

Object *asObject(Value value) {
  if (!isObject(value)) {
    panic("Expected Object but got %s", getValueKindName(value));
  }
  return value.as.object;
}

static Status callCFunction(CFunction *cf, i16 argc, Value *argv, Value *out) {
  if (cf->maxArity == 0) {
    if (cf->arity != argc) {
      panic("Expected %d arguments but got %d", cf->arity, argc);
    }
  } else {
    if (argc < cf->arity || argc > cf->maxArity) {
      panic("Expected %d to %d arguments but got %d", cf->arity, cf->maxArity, argc);
    }
  }
  return cf->body(argc, argv, out);
}

static Status callClass(Class *cls, i16 argc, Value *argv, Value *out) {
  if (!cls->constructor) {
    runtimeError("Class %s is not callable", cls->name);
    return STATUS_ERR;
  }
  return callCFunction(cls->constructor, argc, argv, out);
}

Class *getClass(Value x) {
  switch (x.type) {
    case VALUE_SENTINEL:
      return &SENTINEL_CLASS;
    case VALUE_NIL:
      return &NIL_CLASS;
    case VALUE_BOOL:
      return &BOOL_CLASS;
    case VALUE_NUMBER:
      return &NUMBER_CLASS;
    case VALUE_SYMBOL:
      return &SYMBOL_CLASS;
    case VALUE_CFUNCTION:
      return &CFUNCTION_CLASS;
    case VALUE_CLASS:
      return &CLASS_CLASS;
    case VALUE_OBJECT:
      return getClassOfObject(x.as.object);
  }
}

Status callValue(Value function, i16 argc, Value *argv, Value *out) {
  switch (function.type) {
    case VALUE_SENTINEL:
    case VALUE_NIL:
    case VALUE_BOOL:
    case VALUE_NUMBER:
    case VALUE_SYMBOL:
      break;
    case VALUE_CFUNCTION:
      return callCFunction(function.as.cfunction, argc, argv, out);
    case VALUE_CLASS:
      return callClass(function.as.cls, argc, argv, out);
    case VALUE_OBJECT:
      break;
  }
  panic("%s is not callable", getValueKindName(function));
}

Status callMethod(Symbol *name, i16 argc, Value *argv, Value *out) {
  Class *cls = getClass(argv[-1]);
  Value key = symbolValue(name);
  Value method =
      cls == &CLASS_CLASS ? mapGet(cls->staticMethods, key)
                          : mapGet(cls->instanceMethods, key);
  if (isSentinel(method)) {
    if (cls == &CLASS_CLASS) {
      panic("Static method %s not found on class %s", symbolChars(name), cls->name);
    } else {
      panic("Method %s not found on %s instance", symbolChars(name), cls->name);
    }
  }
  return callValue(method, argc, argv, out);
}

void reprValue(String *out, Value value) {
  switch (value.type) {
    case VALUE_SENTINEL:
      msprintf(out, "(sentinel)");
      return;
    case VALUE_NIL:
      msprintf(out, "nil");
      return;
    case VALUE_BOOL:
      msprintf(out, value.as.boolean ? "true" : "false");
      return;
    case VALUE_NUMBER:
      if (value.as.number == (double)(long)value.as.number) {
        msprintf(out, "%ld", (long)value.as.number);
      } else {
        msprintf(out, "%f", value.as.number);
      }
      return;
    case VALUE_SYMBOL:
      msprintf(out, "%s", symbolChars(value.as.symbol));
      return;
    case VALUE_CFUNCTION:
      msprintf(out, "<cfunction %s>", value.as.cfunction->name);
      return;
    case VALUE_CLASS:
      msprintf(out, "<class %s>", value.as.cls->name);
    case VALUE_OBJECT:
      reprObject(out, value.as.object);
      return;
  }
  panic("INVALID VALUE TYPE %s/%d (reprValue)",
        getValueTypeName(value.type),
        value.type);
}

void strValue(String *out, Value value) {
  if (isString(value)) {
    msprintf(out, "%s", stringChars((String *)value.as.object));
  } else {
    reprValue(out, value);
  }
}

void printValue(Value value) {
  String *out = newString("");
  strValue(out, value);
  printf("%s", stringChars(out));
  releaseString(out);
}

ubool eqValue(Value a, Value b) {
  if (a.type != b.type) {
    return UFALSE;
  }
  switch (a.type) {
    case VALUE_SENTINEL:
      return UTRUE;
    case VALUE_NIL:
      return UTRUE;
    case VALUE_BOOL:
      return a.as.boolean == b.as.boolean;
    case VALUE_NUMBER:
      return a.as.number == b.as.number;
    case VALUE_SYMBOL:
      return a.as.symbol == b.as.symbol;
    case VALUE_CFUNCTION:
      return a.as.cfunction == b.as.cfunction;
    case VALUE_CLASS:
      return a.as.cls == b.as.cls;
    case VALUE_OBJECT:
      return eqObject(a.as.object, b.as.object);
  }
  panic("INVALID VALUE TYPE %s/%d (eqValue)",
        getValueTypeName(a.type),
        a.type);
}

static u32 hashPointer(const void *ptr) {
  /* This is basically what Java does for Long */
  u64 bits = (u64)ptr;
  return (u32)(bits ^ (bits >> 32));
}

u32 hashValue(Value a) {
  /* hash values for bool taken from Java */
  switch (a.type) {
    case VALUE_SENTINEL:
      break;
    case VALUE_NIL:
      return 17;
    case VALUE_BOOL:
      return a.as.boolean ? 1231 : 1237;
    case VALUE_NUMBER: {
      double x = a.as.number;
      union {
        double number;
        u32 parts[2];
      } pun;
      i32 ix = (i32)x;
      if (x == (double)ix) {
        return (u32)ix;
      }
      pun.number = x;
      /* TODO: smarter hashing */
      return pun.parts[0] ^ pun.parts[1];
    }
    case VALUE_SYMBOL:
      return symbolHash(a.as.symbol);
    case VALUE_CFUNCTION:
      return hashPointer(a.as.cfunction);
    case VALUE_CLASS:
      return hashPointer(a.as.cls);
    case VALUE_OBJECT:
      return hashObject(a.as.object);
  }
  panic("INVALID VALUE TYPE %s (hashValue)", getValueTypeName(a.type));
}

void freezeValue(Value a) {
  if (a.type == VALUE_OBJECT) {
    switch (a.as.object->type) {
      case OBJECT_STRING:
        freezeString((String *)a.as.object);
        break;
      case OBJECT_LIST:
        freezeList((List *)a.as.object);
        break;
      case OBJECT_MAP:
        freezeMap((Map *)a.as.object);
        break;
      case OBJECT_NATIVE:
        /* TODO: allow freezign */
        panic("Native objects cannot (yet) be frozen");
    }
  }
}

ubool testValue(Value a) {
  switch (a.type) {
    case VALUE_SENTINEL:
      return UFALSE;
    case VALUE_NIL:
      return UFALSE;
    case VALUE_BOOL:
      return a.as.boolean;
    case VALUE_NUMBER:
      return a.as.number != 0;
    case VALUE_SYMBOL:
      return a.as.symbol != NULL;
    case VALUE_CFUNCTION:
      return UTRUE;
    case VALUE_CLASS:
      return UTRUE;
    case VALUE_OBJECT:
      return testObject(a.as.object);
  }
  panic("INVALID VALUE TYPE %d (testValue)", a.type);
}

Value negateValue(Value a) {
  switch (a.type) {
    case VALUE_NUMBER:
      return numberValue(-a.as.number);
    default:
      break;
  }
  panic("Unsupported operand %s (negateValue)", getValueKindName(a));
}

Value addValues(Value a, Value b) {
  switch (a.type) {
    case VALUE_NUMBER:
      return numberValue(a.as.number + asNumber(b));
    case VALUE_OBJECT:
      switch (a.as.object->type) {
        case OBJECT_STRING:
          return stringValue(addStrings((String *)a.as.object, asString(b)));
        default:
          break;
      }
    default:
      break;
  }
  panic("Unsupported operand %s (addValues)", getValueKindName(a));
}

Value subtractValues(Value a, Value b) {
  switch (a.type) {
    case VALUE_NUMBER:
      return numberValue(a.as.number - asNumber(b));
    default:
      break;
  }
  panic("Unsupported operand %s (subtractValues)", getValueKindName(a));
}

Value multiplyValues(Value a, Value b) {
  switch (a.type) {
    case VALUE_NUMBER:
      return numberValue(a.as.number + asNumber(b));
    default:
      break;
  }
  panic("Unsupported operand %s (multiplyValues)", getValueKindName(a));
}

static double modulo(double a, double b) {
  double r = fmod(a, b);
  return (r != 0 && ((r > 0) != (b > 0))) ? (r + b) : r;
}

Value moduloValues(Value a, Value b) {
  switch (a.type) {
    case VALUE_NUMBER: {
      double rhs = asNumber(b);
      if (rhs == 0) {
        panic("Division by zero (moduloValues)");
      }
      return numberValue(modulo(a.as.number, rhs));
    }
    default:
      break;
  }
  panic("Unsupported operand %s (moduloValues)", getValueKindName(a));
}

Value divideValues(Value a, Value b) {
  switch (a.type) {
    case VALUE_NUMBER: {
      double rhs = asNumber(b);
      if (rhs == 0) {
        panic("Division by zero (divideValues)");
      }
      return numberValue(a.as.number / rhs);
    }
    default:
      break;
  }
  panic("Unsupported operand %s (divideValues)", getValueKindName(a));
}

Value floorDivideValues(Value a, Value b) {
  switch (a.type) {
    case VALUE_NUMBER: {
      double rhs = asNumber(b);
      if (rhs == 0) {
        panic("Division by zero (floorDivideValues)");
      }
      return numberValue(floor(a.as.number / rhs));
    }
    default:
      break;
  }
  panic("Unsupported operand %s (floorDivideValues)", getValueKindName(a));
}

Class *newClass(const char *name) {
  Class *cls = (Class *)calloc(1, sizeof(Class));
  cls->name = name;
  return cls;
}

void classAddStaticMethod(Class *cls, Symbol *name, Value method) {
  retainValue(method);
  mapSet(cls->staticMethods, symbolValue(name), method);
}

void classAddInstanceMethod(Class *cls, Symbol *name, Value method) {
  const CommonSymbols *cs = getCommonSymbols();
  retainValue(method);
  if (name == cs->init) {
    cls->cachedMethods.init = method;
  }
  if (name == cs->repr) {
    cls->cachedMethods.repr = method;
  }
  mapSet(cls->instanceMethods, symbolValue(name), method);
}

void initStaticClass(Class *cls) {
  CFunction **cf;

  if (cls->nativeStaticMethods) {
    for (cf = cls->nativeStaticMethods; *cf; cf++) {
      if (!cls->staticMethods) {
        cls->staticMethods = newMap();
      }
      mapSet(cls->staticMethods,
             symbolValue(newSymbol((*cf)->name)),
             cfunctionValue(*cf));
    }
  }

  if (cls->nativeInstanceMethods) {
    for (cf = cls->nativeInstanceMethods; *cf; cf++) {
      if (!cls->instanceMethods) {
        cls->instanceMethods = newMap();
      }
      mapSet(cls->instanceMethods,
             symbolValue(newSymbol((*cf)->name)),
             cfunctionValue(*cf));
    }
  }
}

#include "mtots2value.h"

#include <stdio.h>

#include "mtots1err.h"
#include "mtots2structs.h"

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
    case VALUE_OBJECT:
      return "Object";
  }
  return "INVALID_VALUE_TYPE";
}

const char *getValueKindName(Value value) {
  if (value.type == VALUE_OBJECT) {
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

Object *asObject(Value value) {
  if (!isObject(value)) {
    panic("Expected Object but got %s", getValueKindName(value));
  }
  return value.as.object;
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
    case VALUE_OBJECT:
      reprObject(out, value.as.object);
      return;
  }
  panic("INVALID VALUE TYPE %s/%d (reprValue)",
        getValueTypeName(value.type),
        value.type);
}

void printValue(Value value) {
  String *out = newString("");
  reprValue(out, value);
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
    case VALUE_OBJECT:
      return eqObject(a.as.object, b.as.object);
  }
  panic("INVALID VALUE TYPE %s/%d (eqValue)",
        getValueTypeName(a.type),
        a.type);
}
u32 hashValue(Value a);

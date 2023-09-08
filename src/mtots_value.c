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
  if (!IS_BOOL(value)) {
    panic("Expected Bool but got %s", getKindName(value));
  }
  return value.as.boolean;
}
double asNumber(Value value) {
  if (!IS_NUMBER(value)) {
    panic("Expected Number but got %s", getKindName(value));
  }
  return value.as.number;
}
Symbol *asSymbol(Value value) {
  if (!IS_SYMBOL(value)) {
    panic("Expected Symbol but got %s", getKindName(value));
  }
  return value.as.symbol;
}
String *asString(Value value) {
  if (!IS_STRING(value)) {
    panic("Expected String but got %s", getKindName(value));
  }
  return value.as.string;
}
CFunction *asCFunction(Value value) {
  if (!IS_CFUNCTION(value)) {
    panic("Expected CFunction but got %s", getKindName(value));
  }
  return value.as.cfunction;
}
Obj *asObj(Value value) {
  if (!IS_OBJ(value)) {
    panic("Expected Obj but got %s", getKindName(value));
  }
  return value.as.obj;
}

Value NIL_VAL(void) {
  Value v = {VAL_NIL};
  return v;
}
Value BOOL_VAL(ubool value) {
  Value v = {VAL_BOOL};
  v.as.boolean = value != 0;
  return v;
}
Value NUMBER_VAL(double value) {
  Value v = {VAL_NUMBER};
  v.as.number = value;
  return v;
}
Value STRING_VAL(String *string) {
  Value v = {VAL_STRING};
  v.as.string = string;
  return v;
}
Value CFUNCTION_VAL(CFunction *func) {
  Value v = {VAL_CFUNCTION};
  v.as.cfunction = func;
  return v;
}
Value SENTINEL_VAL(Sentinel sentinel) {
  Value v = {VAL_SENTINEL};
  v.as.sentinel = sentinel;
  return v;
}
Value OBJ_VAL_EXPLICIT(Obj *object) {
  Value v = {VAL_OBJ};
  v.as.obj = object;
  return v;
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
      return "nil";
    case VAL_BOOL:
      return "bool";
    case VAL_NUMBER:
      return "number";
    case VAL_SYMBOL:
      return "symbol";
    case VAL_STRING:
      return "string";
    case VAL_CFUNCTION:
      return "cfunction";
    case VAL_SENTINEL:
      return "sentinel";
    case VAL_OBJ:
      switch (value.as.obj->type) {
        case OBJ_CLASS:
          return "class";
        case OBJ_CLOSURE:
          return "closure";
        case OBJ_THUNK:
          return "thunk";
        case OBJ_INSTANCE:
          return "instance";
        case OBJ_BUFFER:
          return "buffer";
        case OBJ_LIST:
          return "list";
        case OBJ_FROZEN_LIST:
          return "frozenList";
        case OBJ_DICT:
          return "dict";
        case OBJ_FROZEN_DICT:
          return "frozendict";
        case OBJ_NATIVE:
          return AS_NATIVE_UNSAFE(value)->descriptor->name;
        case OBJ_UPVALUE:
          return "upvalue";
        default:
          return "<unrecognized-object>";
      }
  }
  return "<unrecognized-value>";
}

ubool typePatternMatch(TypePattern pattern, Value value) {
  switch (pattern.type) {
    case TYPE_PATTERN_ANY:
      return UTRUE;
    case TYPE_PATTERN_STRING_OR_NIL:
      if (IS_NIL(value)) {
        return UTRUE;
      }
      /* fallthrough */
    case TYPE_PATTERN_STRING:
      return IS_STRING(value);
    case TYPE_PATTERN_BUFFER_OR_NIL:
      return IS_NIL(value) || IS_BUFFER(value);
    case TYPE_PATTERN_BUFFER:
      return IS_BUFFER(value);
    case TYPE_PATTERN_BOOL:
      return IS_BOOL(value);
    case TYPE_PATTERN_NUMBER_OR_NIL:
      if (IS_NIL(value)) {
        return UTRUE;
      }
      /* fallthrough */
    case TYPE_PATTERN_NUMBER:
      return IS_NUMBER(value);
    case TYPE_PATTERN_LIST_OR_NIL:
      if (IS_NIL(value)) {
        return UTRUE;
      }
      /* fallthrough */
    case TYPE_PATTERN_LIST:
      return IS_LIST(value);
    case TYPE_PATTERN_LIST_NUMBER_OR_NIL:
      if (IS_NIL(value)) {
        return UTRUE;
      }
      /* fallthrough */
    case TYPE_PATTERN_LIST_NUMBER: {
      /* List of Numbers */
      ObjList *list;
      size_t i;
      if (!IS_LIST(value)) {
        return UFALSE;
      }
      list = AS_LIST_UNSAFE(value);
      for (i = 0; i < list->length; i++) {
        if (!IS_NUMBER(list->buffer[i])) {
          return UFALSE;
        }
      }
      return UTRUE;
    }
    case TYPE_PATTERN_LIST_LIST_NUMBER: {
      /* List of List of Numbers */
      ObjList *outerList;
      size_t i;
      if (!IS_LIST(value)) {
        return UFALSE;
      }
      outerList = AS_LIST_UNSAFE(value);
      for (i = 0; i < outerList->length; i++) {
        size_t j;
        ObjList *innerList;
        if (!IS_LIST(outerList->buffer[i])) {
          return UFALSE;
        }
        innerList = AS_LIST_UNSAFE(outerList->buffer[i]);
        for (j = 0; j < innerList->length; j++) {
          if (!IS_NUMBER(innerList->buffer[j])) {
            return UFALSE;
          }
        }
      }
      return UTRUE;
    }
    case TYPE_PATTERN_FROZEN_LIST:
      return IS_FROZEN_LIST(value);
    case TYPE_PATTERN_DICT:
      return IS_DICT(value);
    case TYPE_PATTERN_FROZEN_DICT:
      return IS_FROZEN_DICT(value);
    case TYPE_PATTERN_CLASS:
      return IS_CLASS(value);
    case TYPE_PATTERN_NATIVE_OR_NIL:
      if (IS_NIL(value)) {
        return UTRUE;
      }
      /* fallthrough */
    case TYPE_PATTERN_NATIVE:
      return IS_NATIVE(value) && (pattern.nativeTypeDescriptor == NULL ||
                                  AS_NATIVE_UNSAFE(value)->descriptor == pattern.nativeTypeDescriptor);
  }
  panic("Unrecognized TypePattern type %d", pattern.type);
  return UFALSE;
}

const char *getTypePatternName(TypePattern pattern) {
  switch (pattern.type) {
    case TYPE_PATTERN_ANY:
      return "any";
    case TYPE_PATTERN_STRING_OR_NIL:
      return "(String|nil)";
    case TYPE_PATTERN_STRING:
      return "String";
    case TYPE_PATTERN_BUFFER_OR_NIL:
      return "(Buffer|nil)";
    case TYPE_PATTERN_BUFFER:
      return "Buffer";
    case TYPE_PATTERN_BOOL:
      return "bool";
    case TYPE_PATTERN_NUMBER_OR_NIL:
      return "(number|nil)";
    case TYPE_PATTERN_NUMBER:
      return "number";
    case TYPE_PATTERN_LIST_OR_NIL:
      return "(list|nil)";
    case TYPE_PATTERN_LIST:
      return "list";
    case TYPE_PATTERN_LIST_NUMBER_OR_NIL:
      return "(list[number]|nil)";
    case TYPE_PATTERN_LIST_NUMBER:
      return "list[number]";
    case TYPE_PATTERN_LIST_LIST_NUMBER:
      return "list[list[number]]";
    case TYPE_PATTERN_FROZEN_LIST:
      return "frozenList";
    case TYPE_PATTERN_DICT:
      return "dict";
    case TYPE_PATTERN_FROZEN_DICT:
      return "frozendict";
    case TYPE_PATTERN_CLASS:
      return "class";
    case TYPE_PATTERN_NATIVE_OR_NIL:
      return pattern.nativeTypeDescriptor ? ((NativeObjectDescriptor *)pattern.nativeTypeDescriptor)->name : "(native|nil)";
    case TYPE_PATTERN_NATIVE:
      return pattern.nativeTypeDescriptor ? ((NativeObjectDescriptor *)pattern.nativeTypeDescriptor)->name : "native";
  }
  panic("Unrecognized TypePattern type %d", pattern.type);
  return UFALSE;
}

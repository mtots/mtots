#include "mtots_ops.h"
#include "mtots_vm.h"

#include <stdlib.h>
#include <string.h>

ubool valuesIs(Value a, Value b) {
  if (a.type != b.type) {
    return UFALSE;
  }
  switch (a.type) {
    case VAL_NIL: return UTRUE;
    case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_SYMBOL: return a.as.symbol == b.as.symbol;
    case VAL_STRING: return AS_STRING(a) == AS_STRING(b);
    case VAL_BUILTIN: return a.as.builtin == b.as.builtin;
    case VAL_CFUNCTION: return AS_CFUNCTION(a) == AS_CFUNCTION(b);
    case VAL_SENTINEL: return AS_SENTINEL(a) == AS_SENTINEL(b);
    case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
  }
  abort();
  return UFALSE; /* Unreachable */
}

ubool mapsEqual(Map *a, Map *b) {
  MapIterator di;
  MapEntry *entry;
  if (a->size != b->size) {
    return UFALSE;
  }
  initMapIterator(&di, a);
  while (mapIteratorNext(&di, &entry)) {
    Value key = entry->key;
    if (!IS_EMPTY_KEY(key)) {
      Value value1 = entry->value, value2;
      if (!mapGet(b, key, &value2)) {
        return UFALSE;
      }
      if (!valuesEqual(value1, value2)) {
        return UFALSE;
      }
    }
  }
  return UTRUE;
}

ubool valuesEqual(Value a, Value b) {
  if (a.type != b.type) {
    return UFALSE;
  }
  switch (a.type) {
    case VAL_NIL: return UTRUE;
    case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_SYMBOL: return a.as.symbol == b.as.symbol;
    case VAL_STRING: return AS_STRING(a) == AS_STRING(b);
    case VAL_BUILTIN: return a.as.builtin == b.as.builtin;
    case VAL_CFUNCTION: return AS_CFUNCTION(a) == AS_CFUNCTION(b);
    case VAL_SENTINEL: return AS_SENTINEL(a) == AS_SENTINEL(b);
    case VAL_OBJ: {
      Obj *objA = AS_OBJ(a);
      Obj *objB = AS_OBJ(b);
      if (objA->type != objB->type) {
        return UFALSE;
      }
      if (objA == objB) {
        return UTRUE;
      }
      switch (objA->type) {
        case OBJ_BUFFER: {
          ObjBuffer *bA = (ObjBuffer*)objA, *bB = (ObjBuffer*)objB;
          if (bA->handle.length != bB->handle.length) {
            return UFALSE;
          }
          return memcmp(bA->handle.data, bB->handle.data, bA->handle.length) == 0;
        }
        case OBJ_LIST: {
          ObjList *listA = (ObjList*)objA, *listB = (ObjList*)objB;
          size_t i;
          if (listA->length != listB->length) {
            return UFALSE;
          }
          for (i = 0; i < listA->length; i++) {
            if (!valuesEqual(listA->buffer[i], listB->buffer[i])) {
              return UFALSE;
            }
          }
          return UTRUE;
        }
        case OBJ_DICT: {
          ObjDict *dictA = (ObjDict*)objA, *dictB = (ObjDict*)objB;
          return mapsEqual(&dictA->map, &dictB->map);
        }
        case OBJ_NATIVE: {
          /* TODO: Consider supporting overridable __eq__ */
          ObjNative *na = (ObjNative*)objA;
          ObjNative *nb = (ObjNative*)objB;
          if (na->descriptor != nb->descriptor) {
            return UFALSE;
          }
          return objA == objB;
        }
        default: return objA == objB;
      }
    }
  }
  abort();
  return UFALSE; /* Unreachable */
}

ubool valueLessThan(Value a, Value b) {
  if (a.type != b.type) {
    panic(
      "'<' requires values of the same type but got %s and %s",
      getKindName(a), getKindName(b));
  }
  switch (a.type) {
    case VAL_NIL: return UFALSE;
    case VAL_BOOL: return AS_BOOL(a) < AS_BOOL(b);
    case VAL_NUMBER: return AS_NUMBER(a) < AS_NUMBER(b);
    case VAL_SYMBOL: {
      Symbol *sa = a.as.symbol;
      Symbol *sb = b.as.symbol;
      if (sa == sb) {
        return UFALSE;
      }
      return strcmp(getSymbolChars(sa), getSymbolChars(sb)) < 0;
    }
    case VAL_STRING: {
      /* Use u8 instead of char when comparing to ensure that
       * larger code points compare larger */
      String *strA = AS_STRING(a);
      String *strB = AS_STRING(b);
      size_t lenA = strA->byteLength;
      size_t lenB = strB->byteLength;
      size_t len = lenA < lenB ? lenA : lenB;
      size_t i;
      const u8 *charsA = (u8*)strA->chars;
      const u8 *charsB = (u8*)strB->chars;
      if (strA == strB) {
        return UFALSE;
      }
      for (i = 0; i < len; i++) {
        if (charsA[i] != charsB[i]) {
          return charsA[i] < charsB[i];
        }
      }
      return lenA < lenB;
    }
    case VAL_BUILTIN: break;
    case VAL_CFUNCTION: break;
    case VAL_SENTINEL: break;
    case VAL_OBJ: {
      Obj *objA = AS_OBJ(a);
      Obj *objB = AS_OBJ(b);
      if (objA->type != objB->type) {
        panic(
          "'<' requires values of the same type but got %s and %s",
          getKindName(a), getKindName(b));
      }
      switch (objA->type) {
        case OBJ_LIST: {
          ObjList *listA = (ObjList*)objA;
          ObjList *listB = (ObjList*)objB;
          size_t lenA = listA->length;
          size_t lenB = listB->length;
          size_t len = lenA < lenB ? lenA : lenB;
          size_t i;
          Value *bufA = listA->buffer;
          Value *bufB = listB->buffer;
          if (listA == listB) {
            return UFALSE;
          }
          for (i = 0; i < len; i++) {
            if (!valuesEqual(bufA[i], bufB[i])) {
              return valueLessThan(bufA[i], bufB[i]);
            }
          }
          return lenA < lenB;
        }
        case OBJ_FROZEN_LIST: {
          ObjFrozenList *frozenListA = (ObjFrozenList*)objA;
          ObjFrozenList *frozenListB = (ObjFrozenList*)objB;
          size_t lenA = frozenListA->length;
          size_t lenB = frozenListB->length;
          size_t len = lenA < lenB ? lenA : lenB;
          size_t i;
          Value *bufA = frozenListA->buffer;
          Value *bufB = frozenListB->buffer;
          if (frozenListA == frozenListB) {
            return UFALSE;
          }
          for (i = 0; i < len; i++) {
            if (!valuesEqual(bufA[i], bufB[i])) {
              return valueLessThan(bufA[i], bufB[i]);
            }
          }
          return lenA < lenB;
        }
        default: break;
      }
      break;
    }
  }
  panic("%s does not support '<'", getKindName(a));
  return UFALSE;
}

typedef struct SortEntry {
  Value key;
  Value value;
} SortEntry;

/* Basic mergesort.
 * TODO: Consider using timsort instead */
void sortList(ObjList *list, ObjList *keys) {
  SortEntry *buffer, *src, *dst;
  size_t i, len = list->length, width;
  /* TODO: this check is untested - come back and
    * actually think this through when there is more bandwidth */
  if (len >= ((size_t)(-1)) / 4) {
    panic("sortList(): list too long (%lu)", (long) len);
  }
  if (keys != NULL && len != keys->length) {
    panic(
      "sortList(): item list and key list lengths do not match: "
      "%lu, %lu",
      (unsigned long) list->length, (unsigned long) keys->length);
  }
  /* TODO: Consider falling back to an in-place sorting algorithm
   * if we run do not have enough memory for the buffer (maybe qsort?) */
  /* NOTE: We call malloc directly instead of ALLOCATE because
   * I don't want to potentially trigger a GC call here.
   * And besides, none of the memory we allocate in this call should
   * outlive this call */
  buffer = malloc(sizeof(SortEntry) * 2 * len);
  src = buffer;
  dst = buffer + len;
  for (i = 0; i < len; i++) {
    src[i].key = keys == NULL ? list->buffer[i] : keys->buffer[i];
    src[i].value = list->buffer[i];
  }

  /* bottom up merge sort */
  for (width = 1; width < len; width *= 2) {
    for (i = 0; i < len; i += 2 * width) {
      size_t low  = i;
      size_t mid  = i +     width < len ? i +     width : len;
      size_t high = i + 2 * width < len ? i + 2 * width : len;
      size_t a = low, b = mid, j;
      for (j = low; j < high; j++) {
        dst[j] =
          b < high && (a >= mid || valueLessThan(src[b].key, src[a].key)) ?
            src[b++] : src[a++];
      }
    }
    {
      SortEntry *tmp = src;
      src = dst;
      dst = tmp;
    }
  }

  /* copy contents back into the list */
  for (i = 0; i < len; i++) {
    list->buffer[i] = src[i].value;
  }

  free(buffer);
}

ubool sortListWithKeyFunc(ObjList *list, Value keyfunc) {
  if (!IS_NIL(keyfunc)) {
    ObjList *keys = newList(list->length);
    size_t i = 0;
    push(LIST_VAL(keys));
    for (i = 0; i < list->length; i++) {
      push(keyfunc);
      push(list->buffer[i]);
      if (!callFunction(1)) {
        return UFALSE;
      }
      keys->buffer[i] = pop();
    }
    sortList(list, keys);
    pop(); /* keys */
    return UTRUE;
  }

  /* If keyfunc is nil, we can call sortList directly */
  sortList(list, NULL);
  return UTRUE;
}

static ubool mapRepr(Buffer *out, Map *map) {
  MapIterator mi;
  MapEntry *entry;
  size_t i;

  bputchar(out, '{');
  initMapIterator(&mi, map);
  for (i = 0; mapIteratorNext(&mi, &entry); i++) {
    if (i > 0) {
      bputchar(out, ',');
      bputchar(out, ' ');
    }
    if (!valueRepr(out, entry->key)) {
      return UFALSE;
    }
    if (!IS_NIL(entry->value)) {
      bputchar(out, ':');
      bputchar(out, ' ');
      if (!valueRepr(out, entry->value)) {
        return UFALSE;
      }
    }
  }
  bputchar(out, '}');
  return UTRUE;
}

ubool valueRepr(Buffer *out, Value value) {
  switch (value.type) {
    case VAL_NIL: bprintf(out, "nil"); return UTRUE;
    case VAL_BOOL: bprintf(out, AS_BOOL(value) ? "true" : "false"); return UTRUE;
    case VAL_NUMBER: bputnumber(out, AS_NUMBER(value)); return UTRUE;
    case VAL_SYMBOL: bprintf(out, "<Symbol %s>", getSymbolChars(value.as.symbol)); return UTRUE;
    case VAL_STRING: {
      String *str = AS_STRING(value);
      bputchar(out, '"');
      if (!escapeString2(out, str->chars, str->byteLength, NULL)) {
        return UFALSE;
      }
      bputchar(out, '"');
      return UTRUE;
    }
    case VAL_BUILTIN: bprintf(out, "<builtin function %s>", value.as.builtin->name); return UTRUE;
    case VAL_CFUNCTION: bprintf(out, "<function %s>", AS_CFUNCTION(value)->name); return UTRUE;
    case VAL_SENTINEL: bprintf(out, "<sentinel %d>", AS_SENTINEL(value)); return UTRUE;
    case VAL_OBJ: {
      Obj *obj = AS_OBJ(value);
      switch (obj->type) {
        case OBJ_CLASS: bprintf(out, "<class %s>", AS_CLASS(value)->name->chars); return UTRUE;
        case OBJ_CLOSURE:
          bprintf(out, "<function %s>", AS_CLOSURE(value)->thunk->name->chars);
          return UTRUE;
        case OBJ_THUNK: bprintf(out, "<thunk %s>", AS_THUNK(value)->name->chars); return UTRUE;
        case OBJ_INSTANCE:
          if (AS_INSTANCE(value)->klass->isModuleClass) {
            bprintf(out, "<module %s>", AS_INSTANCE(value)->klass->name->chars);
          } else if (classHasMethod(getClassOfValue(value), vm.reprString)) {
            Value resultValue;
            String *resultString;
            push(value);
            if (!callMethod(vm.reprString, 0)) {
              return UFALSE;
            }
            resultValue = pop();
            if (!IS_STRING(resultValue)) {
              ObjClass *cls = getClassOfValue(value);
              runtimeError(
                "%s.__repr__() must return a String but returned %s",
                cls->name->chars,
                getKindName(resultValue));
              return UFALSE;
            }
            resultString = AS_STRING(resultValue);
            bputstrlen(out, resultString->chars, resultString->byteLength);
          } else {
            bprintf(out, "<%s instance>", AS_INSTANCE(value)->klass->name->chars);
          }
          return UTRUE;
        case OBJ_BUFFER: {
          ObjBuffer *bufObj = AS_BUFFER(value);
          Buffer *buf = &bufObj->handle;
          StringEscapeOptions opts;
          initStringEscapeOptions(&opts);
          opts.shorthandControlCodes = UFALSE;
          opts.tryUnicode = UFALSE;
          bputchar(out, 'b');
          bputchar(out, '"');
          escapeString2(out, (const char*)buf->data, buf->length, &opts);
          bputchar(out, '"');
          return UTRUE;
        }
        case OBJ_LIST: {
          ObjList *list = AS_LIST(value);
          size_t i, len = list->length;
          bputchar(out, '[');
          for (i = 0; i < len; i++) {
            if (i > 0) {
              bputchar(out, ',');
              bputchar(out, ' ');
            }
            if (!valueRepr(out, list->buffer[i])) {
              return UFALSE;
            }
          }
          bputchar(out, ']');
          return UTRUE;
        }
        case OBJ_FROZEN_LIST: {
          ObjFrozenList *frozenList = AS_FROZEN_LIST(value);
          size_t i, len = frozenList->length;
          bputchar(out, '(');
          for (i = 0; i < len; i++) {
            if (i > 0) {
              bputchar(out, ',');
              bputchar(out, ' ');
            }
            if (!valueRepr(out, frozenList->buffer[i])) {
              return UFALSE;
            }
          }
          bputchar(out, ')');
          return UTRUE;
        }
        case OBJ_DICT: {
          ObjDict *dict = AS_DICT(value);
          return mapRepr(out, &dict->map);
        }
        case OBJ_FROZEN_DICT: {
          ObjFrozenDict *dict = AS_FROZEN_DICT(value);
          bputstr(out, "final");
          return mapRepr(out, &dict->map);
        }
        case OBJ_NATIVE:
          if (classHasMethod(getClassOfValue(value), vm.reprString)) {
            Value resultValue;
            String *resultString;
            push(value);
            if (!callMethod(vm.reprString, 0)) {
              return UFALSE;
            }
            resultValue = pop();
            if (!IS_STRING(resultValue)) {
              ObjClass *cls = getClassOfValue(value);
              runtimeError(
                "%s.__repr__() must return a String but returned %s",
                cls->name->chars,
                getKindName(resultValue));
              return UFALSE;
            }
            resultString = AS_STRING(resultValue);
            bputstrlen(out, resultString->chars, resultString->byteLength);
          } else {
            bprintf(out, "<%s native-instance>",
              AS_NATIVE(value)->descriptor->klass->name->chars);
          }
          return UTRUE;
        case OBJ_UPVALUE:
          bprintf(out, "<upvalue>");
          return UTRUE;
      }
    }
  }
  panic("unrecognized value type %s", getKindName(value));
  return UFALSE;
}

ubool valueStr(Buffer *out, Value value) {
  if (IS_STRING(value)) {
    String *string = AS_STRING(value);
    bputstrlen(out, string->chars, string->byteLength);
    return UTRUE;
  }
  return valueRepr(out, value);
}

ubool strMod(Buffer *out, const char *format, ObjList *args) {
  const char *p;
  size_t j;

  for ((void)(p = format), j = 0; *p != '\0'; p++) {
    if (*p == '%') {
      Value item;
      p++;
      if (*p == '%') {
        bputchar(out, '%');
        continue;
      }
      if (j >= args->length) {
        runtimeError("Not enough arguments for format string");
        return UFALSE;
      }
      item = args->buffer[j++];
      switch (*p) {
        case 's':
          if (!valueStr(out, item)) {
            return UFALSE;
          }
          break;
        case 'r':
          if (!valueRepr(out, item)) {
            return UFALSE;
          }
          break;
        case '\0':
          runtimeError("missing format indicator");
          return UFALSE;
        default:
          runtimeError("invalid format indicator '%%%c'", *p);
          return UFALSE;
      }
    } else {
      bputchar(out, *p);
    }
  }

  return UTRUE;
}

ubool valueLen(Value recv, size_t *out) {
  if (IS_STRING(recv)) {
    *out = AS_STRING(recv)->codePointCount;
    return UTRUE;
  } else if (IS_OBJ(recv)) {
    switch (AS_OBJ(recv)->type) {
      case OBJ_BUFFER:
        *out = AS_BUFFER(recv)->handle.length;
        return UTRUE;
      case OBJ_LIST:
        *out = AS_LIST(recv)->length;
        return UTRUE;
      case OBJ_FROZEN_LIST:
        *out = AS_FROZEN_LIST(recv)->length;
        return UTRUE;
      case OBJ_DICT:
        *out = AS_DICT(recv)->map.size;
        return UTRUE;
      case OBJ_FROZEN_DICT:
        *out = AS_FROZEN_DICT(recv)->map.size;
        return UTRUE;
      default: {
        Value value;
        push(recv);
        if (!callMethod(vm.lenString, 0)) {
          return UFALSE;
        }
        value = pop();
        if (!IS_NUMBER(value)) {
          runtimeError(
            "__len__ did not return a number (got %s)",
            getKindName(value));
          return UFALSE;
        }
        *out = AS_NUMBER(value);
        return UTRUE;
      }
    }
  }
  runtimeError(
    "object of kind '%s' has no len()",
    getKindName(recv));
  return UFALSE;
}

static ubool isIterator(Value value) {
  if (IS_OBJ(value)) {
    switch (AS_OBJ(value)->type) {
      case OBJ_CLOSURE: return AS_CLOSURE(value)->thunk->arity == 0;
      case OBJ_NATIVE: {
        CFunction *call = AS_NATIVE(value)->descriptor->klass->call;
        return call && call->arity == 0;
      }
      default: break;
    }
  }
  return UFALSE;
}

ubool valueIter(Value iterable, Value *out) {
  if (isIterator(iterable)) {
    *out = iterable;
    return UTRUE;
  }
  push(iterable);
  if (!callMethod(vm.iterString, 0)) {
    return UFALSE;
  }
  *out = pop();
  return UTRUE;
}

ubool valueIterNext(Value iterator, Value *out) {
  push(iterator);
  if (!callFunction(0)) {
    return UFALSE;
  }
  *out = pop();
  return UTRUE;
}

ubool valueFastIter(Value iterable, Value *out) {
  return valueIter(iterable, out);
}

ubool valueFastIterNext(Value *iterator, Value *out) {
  return valueIterNext(*iterator, out);
}

ubool valueGetItem(Value owner, Value key, Value *out) {
  if (IS_LIST(owner) && IS_NUMBER(key)) {
    ObjList *list = AS_LIST(owner);
    size_t i = AS_INDEX(key, list->length);
    *out = list->buffer[i];
    return UTRUE;
  }
  push(owner);
  push(key);
  if (!callMethod(vm.getitemString, 1)) {
    return UFALSE;
  }
  *out = pop();
  return UTRUE;
}

ubool valueSetItem(Value owner, Value key, Value value) {
  if (IS_LIST(owner) && IS_NUMBER(key)) {
    ObjList *list = AS_LIST(owner);
    size_t i = AS_INDEX(key, list->length);
    list->buffer[i] = value;
    return UTRUE;
  }
  push(owner);
  push(key);
  push(value);
  if (!callMethod(vm.setitemString, 2)) {
    return UFALSE;
  }
  pop(); /* method call result */
  return UTRUE;
}

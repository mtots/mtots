#include "mtots_ops.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

static ubool rangesEqual(Range a, Range b) {
  return a.start == b.start && a.stop == b.stop && a.step == b.step;
}

static ubool vectorsEqual(Vector a, Vector b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

ubool valuesIs(Value a, Value b) {
  if (a.type != b.type) {
    return STATUS_ERROR;
  }
  switch (a.type) {
    case VAL_NIL:
      return STATUS_OK;
    case VAL_BOOL:
      return a.as.boolean == b.as.boolean;
    case VAL_NUMBER:
      return a.as.number == b.as.number;
    case VAL_STRING:
      return a.as.string == b.as.string;
    case VAL_CFUNCTION:
      return a.as.cfunction == b.as.cfunction;
    case VAL_SENTINEL:
      return a.as.sentinel == b.as.sentinel;
    case VAL_RANGE:
      return rangesEqual(asRange(a), asRange(b));
    case VAL_RANGE_ITERATOR:
      return rangesEqual(asRange(a), asRange(b));
    case VAL_VECTOR:
      return vectorsEqual(asVector(a), asVector(b));
    case VAL_OBJ:
      return a.as.obj == b.as.obj;
  }
  abort();
}

ubool mapsEqual(Map *a, Map *b) {
  MapIterator di;
  MapEntry *entry;
  if (a->size != b->size) {
    return STATUS_ERROR;
  }
  initMapIterator(&di, a);
  while (mapIteratorNext(&di, &entry)) {
    Value key = entry->key;
    if (!isEmptyKey(key)) {
      Value value1 = entry->value, value2;
      if (!mapGet(b, key, &value2)) {
        return STATUS_ERROR;
      }
      if (!valuesEqual(value1, value2)) {
        return STATUS_ERROR;
      }
    }
  }
  return STATUS_OK;
}

ubool valuesEqual(Value a, Value b) {
  if (a.type != b.type) {
    return STATUS_ERROR;
  }
  switch (a.type) {
    case VAL_NIL:
      return STATUS_OK;
    case VAL_BOOL:
      return a.as.boolean == b.as.boolean;
    case VAL_NUMBER:
      return a.as.number == b.as.number;
    case VAL_STRING:
      return a.as.string == b.as.string;
    case VAL_CFUNCTION:
      return a.as.cfunction == b.as.cfunction;
    case VAL_SENTINEL:
      return a.as.sentinel == b.as.sentinel;
    case VAL_RANGE:
      return rangesEqual(asRange(a), asRange(b));
    case VAL_RANGE_ITERATOR:
      return rangesEqual(asRange(a), asRange(b));
    case VAL_VECTOR:
      return vectorsEqual(asVector(a), asVector(b));
    case VAL_OBJ: {
      Obj *objA = a.as.obj;
      Obj *objB = b.as.obj;
      if (objA->type != objB->type) {
        return STATUS_ERROR;
      }
      if (objA == objB) {
        return STATUS_OK;
      }
      switch (objA->type) {
        case OBJ_BUFFER: {
          ObjBuffer *bA = (ObjBuffer *)objA, *bB = (ObjBuffer *)objB;
          if (bA->handle.length != bB->handle.length) {
            return STATUS_ERROR;
          }
          return memcmp(bA->handle.data, bB->handle.data, bA->handle.length) == 0;
        }
        case OBJ_LIST: {
          ObjList *listA = (ObjList *)objA, *listB = (ObjList *)objB;
          size_t i;
          if (listA->length != listB->length) {
            return STATUS_ERROR;
          }
          for (i = 0; i < listA->length; i++) {
            if (!valuesEqual(listA->buffer[i], listB->buffer[i])) {
              return STATUS_ERROR;
            }
          }
          return STATUS_OK;
        }
        case OBJ_DICT: {
          ObjDict *dictA = (ObjDict *)objA, *dictB = (ObjDict *)objB;
          return mapsEqual(&dictA->map, &dictB->map);
        }
        case OBJ_NATIVE: {
          /* TODO: Consider supporting overridable __eq__ */
          ObjNative *na = (ObjNative *)objA;
          ObjNative *nb = (ObjNative *)objB;
          if (na->descriptor != nb->descriptor) {
            return STATUS_ERROR;
          }
          return objA == objB;
        }
        default:
          return objA == objB;
      }
    }
  }
  abort();
}

ubool valueLessThan(Value a, Value b) {
  if (a.type != b.type) {
    panic(
        "'<' requires values of the same type but got %s and %s",
        getKindName(a), getKindName(b));
  }
  switch (a.type) {
    case VAL_NIL:
      return STATUS_ERROR;
    case VAL_BOOL:
      return a.as.boolean < b.as.boolean;
    case VAL_NUMBER:
      return a.as.number < b.as.number;
    case VAL_STRING: {
      /* Use u8 instead of char when comparing to ensure that
       * larger code points compare larger */
      String *strA = a.as.string;
      String *strB = b.as.string;
      size_t lenA = strA->byteLength;
      size_t lenB = strB->byteLength;
      size_t len = lenA < lenB ? lenA : lenB;
      size_t i;
      const u8 *charsA = (u8 *)strA->chars;
      const u8 *charsB = (u8 *)strB->chars;
      if (strA == strB) {
        return STATUS_ERROR;
      }
      for (i = 0; i < len; i++) {
        if (charsA[i] != charsB[i]) {
          return charsA[i] < charsB[i];
        }
      }
      return lenA < lenB;
    }
    case VAL_CFUNCTION:
      break;
    case VAL_SENTINEL:
      break;
    case VAL_RANGE:
      break;
    case VAL_RANGE_ITERATOR:
      break;
    case VAL_VECTOR: {
      Vector va = asVector(a);
      Vector vb = asVector(b);
      return va.x < vb.x ||
             (va.x == vb.x && (va.y < vb.y || (va.y == vb.y && va.z < vb.z)));
    }
    case VAL_OBJ: {
      Obj *objA = AS_OBJ_UNSAFE(a);
      Obj *objB = AS_OBJ_UNSAFE(b);
      if (objA->type != objB->type) {
        panic(
            "'<' requires values of the same type but got %s and %s",
            getKindName(a), getKindName(b));
      }
      switch (objA->type) {
        case OBJ_LIST: {
          ObjList *listA = (ObjList *)objA;
          ObjList *listB = (ObjList *)objB;
          size_t lenA = listA->length;
          size_t lenB = listB->length;
          size_t len = lenA < lenB ? lenA : lenB;
          size_t i;
          Value *bufA = listA->buffer;
          Value *bufB = listB->buffer;
          if (listA == listB) {
            return STATUS_ERROR;
          }
          for (i = 0; i < len; i++) {
            if (!valuesEqual(bufA[i], bufB[i])) {
              return valueLessThan(bufA[i], bufB[i]);
            }
          }
          return lenA < lenB;
        }
        case OBJ_FROZEN_LIST: {
          ObjFrozenList *frozenListA = (ObjFrozenList *)objA;
          ObjFrozenList *frozenListB = (ObjFrozenList *)objB;
          size_t lenA = frozenListA->length;
          size_t lenB = frozenListB->length;
          size_t len = lenA < lenB ? lenA : lenB;
          size_t i;
          Value *bufA = frozenListA->buffer;
          Value *bufB = frozenListB->buffer;
          if (frozenListA == frozenListB) {
            return STATUS_ERROR;
          }
          for (i = 0; i < len; i++) {
            if (!valuesEqual(bufA[i], bufB[i])) {
              return valueLessThan(bufA[i], bufB[i]);
            }
          }
          return lenA < lenB;
        }
        default:
          break;
      }
      break;
    }
  }
  panic("%s does not support '<'", getKindName(a));
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
    panic("sortList(): list too long (%lu)", (long)len);
  }
  if (keys != NULL && len != keys->length) {
    panic(
        "sortList(): item list and key list lengths do not match: "
        "%lu, %lu",
        (unsigned long)list->length, (unsigned long)keys->length);
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
      size_t low = i;
      size_t mid = i + width < len ? i + width : len;
      size_t high = i + 2 * width < len ? i + 2 * width : len;
      size_t a = low, b = mid, j;
      for (j = low; j < high; j++) {
        dst[j] =
            b < high && (a >= mid || valueLessThan(src[b].key, src[a].key)) ? src[b++] : src[a++];
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
  if (!isNil(keyfunc)) {
    ObjList *keys = newList(list->length);
    size_t i = 0;
    push(valList(keys));
    for (i = 0; i < list->length; i++) {
      push(keyfunc);
      push(list->buffer[i]);
      if (!callFunction(1)) {
        return STATUS_ERROR;
      }
      keys->buffer[i] = pop();
    }
    sortList(list, keys);
    pop(); /* keys */
    return STATUS_OK;
  }

  /* If keyfunc is nil, we can call sortList directly */
  sortList(list, NULL);
  return STATUS_OK;
}

static ubool mapRepr(StringBuilder *out, Map *map) {
  MapIterator mi;
  MapEntry *entry;
  size_t i;

  sbputchar(out, '{');
  initMapIterator(&mi, map);
  for (i = 0; mapIteratorNext(&mi, &entry); i++) {
    if (i > 0) {
      sbputchar(out, ',');
      sbputchar(out, ' ');
    }
    if (!valueRepr(out, entry->key)) {
      return STATUS_ERROR;
    }
    if (!isNil(entry->value)) {
      sbputchar(out, ':');
      sbputchar(out, ' ');
      if (!valueRepr(out, entry->value)) {
        return STATUS_ERROR;
      }
    }
  }
  sbputchar(out, '}');
  return STATUS_OK;
}

ubool valueRepr(StringBuilder *out, Value value) {
  switch (value.type) {
    case VAL_NIL:
      sbprintf(out, "nil");
      return STATUS_OK;
    case VAL_BOOL:
      sbprintf(out, value.as.boolean ? "true" : "false");
      return STATUS_OK;
    case VAL_NUMBER:
      sbputnumber(out, value.as.number);
      return STATUS_OK;
    case VAL_STRING: {
      String *str = value.as.string;
      sbputchar(out, '"');
      if (!escapeString2(out, str->chars, str->byteLength, NULL)) {
        return STATUS_ERROR;
      }
      sbputchar(out, '"');
      return STATUS_OK;
    }
    case VAL_CFUNCTION:
      sbprintf(out, "<function %s>", value.as.cfunction->name);
      return STATUS_OK;
    case VAL_SENTINEL:
      sbprintf(out, "<sentinel %d>", value.as.sentinel);
      return STATUS_OK;
    case VAL_RANGE: {
      Range range = asRange(value);
      sbprintf(out, "Range(%d, %d, %d)", (int)range.start, (int)range.stop, (int)range.stop);
      return STATUS_OK;
    }
    case VAL_RANGE_ITERATOR:
      sbprintf(out, "<RangeIterator instance>");
      return STATUS_OK;
    case VAL_VECTOR: {
      Vector vector = asVector(value);
      sbprintf(out, "Vector(");
      sbputnumber(out, vector.x);
      sbprintf(out, ", ");
      sbputnumber(out, vector.y);
      sbprintf(out, ", ");
      sbputnumber(out, vector.z);
      sbprintf(out, ")");
      return STATUS_OK;
    }
    case VAL_OBJ: {
      Obj *obj = AS_OBJ_UNSAFE(value);
      switch (obj->type) {
        case OBJ_CLASS:
          sbprintf(out, "<class %s>", AS_CLASS_UNSAFE(value)->name->chars);
          return STATUS_OK;
        case OBJ_CLOSURE:
          sbprintf(out, "<function %s>", AS_CLOSURE_UNSAFE(value)->thunk->name->chars);
          return STATUS_OK;
        case OBJ_THUNK:
          sbprintf(out, "<thunk %s>", AS_THUNK_UNSAFE(value)->name->chars);
          return STATUS_OK;
        case OBJ_INSTANCE:
          if (AS_INSTANCE_UNSAFE(value)->klass->isModuleClass) {
            sbprintf(out, "<module %s>", AS_INSTANCE_UNSAFE(value)->klass->name->chars);
          } else if (classHasMethod(getClassOfValue(value), vm.reprString)) {
            Value resultValue;
            String *resultString;
            push(value);
            if (!callMethod(vm.reprString, 0)) {
              return STATUS_ERROR;
            }
            resultValue = pop();
            if (!isString(resultValue)) {
              ObjClass *cls = getClassOfValue(value);
              runtimeError(
                  "%s.__repr__() must return a String but returned %s",
                  cls->name->chars,
                  getKindName(resultValue));
              return STATUS_ERROR;
            }
            resultString = resultValue.as.string;
            sbputstrlen(out, resultString->chars, resultString->byteLength);
          } else {
            sbprintf(out, "<%s instance>", AS_INSTANCE_UNSAFE(value)->klass->name->chars);
          }
          return STATUS_OK;
        case OBJ_BUFFER: {
          ObjBuffer *bufObj = AS_BUFFER_UNSAFE(value);
          Buffer *buf = &bufObj->handle;
          StringEscapeOptions opts;
          initStringEscapeOptions(&opts);
          opts.shorthandControlCodes = UFALSE;
          opts.tryUnicode = UFALSE;
          sbputchar(out, 'b');
          sbputchar(out, '"');
          escapeString2(out, (const char *)buf->data, buf->length, &opts);
          sbputchar(out, '"');
          return STATUS_OK;
        }
        case OBJ_LIST: {
          ObjList *list = AS_LIST_UNSAFE(value);
          size_t i, len = list->length;
          sbputchar(out, '[');
          for (i = 0; i < len; i++) {
            if (i > 0) {
              sbputchar(out, ',');
              sbputchar(out, ' ');
            }
            if (!valueRepr(out, list->buffer[i])) {
              return STATUS_ERROR;
            }
          }
          sbputchar(out, ']');
          return STATUS_OK;
        }
        case OBJ_FROZEN_LIST: {
          ObjFrozenList *frozenList = AS_FROZEN_LIST_UNSAFE(value);
          size_t i, len = frozenList->length;
          sbputchar(out, '(');
          for (i = 0; i < len; i++) {
            if (i > 0) {
              sbputchar(out, ',');
              sbputchar(out, ' ');
            }
            if (!valueRepr(out, frozenList->buffer[i])) {
              return STATUS_ERROR;
            }
          }
          sbputchar(out, ')');
          return STATUS_OK;
        }
        case OBJ_DICT: {
          ObjDict *dict = AS_DICT_UNSAFE(value);
          return mapRepr(out, &dict->map);
        }
        case OBJ_FROZEN_DICT: {
          ObjFrozenDict *dict = AS_FROZEN_DICT_UNSAFE(value);
          sbputstr(out, "final");
          return mapRepr(out, &dict->map);
        }
        case OBJ_NATIVE:
          if (classHasMethod(getClassOfValue(value), vm.reprString)) {
            Value resultValue;
            String *resultString;
            push(value);
            if (!callMethod(vm.reprString, 0)) {
              return STATUS_ERROR;
            }
            resultValue = pop();
            if (!isString(resultValue)) {
              ObjClass *cls = getClassOfValue(value);
              runtimeError(
                  "%s.__repr__() must return a String but returned %s",
                  cls->name->chars,
                  getKindName(resultValue));
              return STATUS_ERROR;
            }
            resultString = resultValue.as.string;
            sbputstrlen(out, resultString->chars, resultString->byteLength);
          } else {
            sbprintf(out, "<%s native-instance>",
                     AS_NATIVE_UNSAFE(value)->descriptor->klass->name->chars);
          }
          return STATUS_OK;
        case OBJ_UPVALUE:
          sbprintf(out, "<upvalue>");
          return STATUS_OK;
      }
    }
  }
  panic("unrecognized value type %s", getKindName(value));
}

ubool valueStr(StringBuilder *out, Value value) {
  if (isString(value)) {
    String *string = value.as.string;
    sbputstrlen(out, string->chars, string->byteLength);
    return STATUS_OK;
  }
  return valueRepr(out, value);
}

ubool strMod(StringBuilder *out, const char *format, ObjList *args) {
  const char *p;
  size_t j;

  for ((void)(p = format), j = 0; *p != '\0'; p++) {
    if (*p == '%') {
      Value item;
      p++;
      if (*p == '%') {
        sbputchar(out, '%');
        continue;
      }
      if (j >= args->length) {
        runtimeError("Not enough arguments for format string");
        return STATUS_ERROR;
      }
      item = args->buffer[j++];
      switch (*p) {
        case 's':
          if (!valueStr(out, item)) {
            return STATUS_ERROR;
          }
          break;
        case 'r':
          if (!valueRepr(out, item)) {
            return STATUS_ERROR;
          }
          break;
        case '\0':
          runtimeError("missing format indicator");
          return STATUS_ERROR;
        default:
          runtimeError("invalid format indicator '%%%c'", *p);
          return STATUS_ERROR;
      }
    } else {
      sbputchar(out, *p);
    }
  }

  return STATUS_OK;
}

ubool valueLen(Value recv, size_t *out) {
  if (isString(recv)) {
    *out = recv.as.string->codePointCount;
    return STATUS_OK;
  } else if (isObj(recv)) {
    switch (AS_OBJ_UNSAFE(recv)->type) {
      case OBJ_BUFFER:
        *out = AS_BUFFER_UNSAFE(recv)->handle.length;
        return STATUS_OK;
      case OBJ_LIST:
        *out = AS_LIST_UNSAFE(recv)->length;
        return STATUS_OK;
      case OBJ_FROZEN_LIST:
        *out = AS_FROZEN_LIST_UNSAFE(recv)->length;
        return STATUS_OK;
      case OBJ_DICT:
        *out = AS_DICT_UNSAFE(recv)->map.size;
        return STATUS_OK;
      case OBJ_FROZEN_DICT:
        *out = AS_FROZEN_DICT_UNSAFE(recv)->map.size;
        return STATUS_OK;
      default: {
        Value value;
        push(recv);
        if (!callMethod(vm.lenString, 0)) {
          return STATUS_ERROR;
        }
        value = pop();
        if (!isNumber(value)) {
          runtimeError(
              "__len__ did not return a number (got %s)",
              getKindName(value));
          return STATUS_ERROR;
        }
        *out = value.as.number;
        return STATUS_OK;
      }
    }
  }
  runtimeError(
      "object of kind '%s' has no len()",
      getKindName(recv));
  return STATUS_ERROR;
}

static ubool isIterator(Value value) {
  if (isObj(value)) {
    switch (AS_OBJ_UNSAFE(value)->type) {
      case OBJ_CLOSURE:
        return AS_CLOSURE_UNSAFE(value)->thunk->arity == 0;
      case OBJ_NATIVE: {
        CFunction *call = AS_NATIVE_UNSAFE(value)->descriptor->klass->call;
        return call && call->arity == 0;
      }
      default:
        break;
    }
  }
  return STATUS_ERROR;
}

Status valueIter(Value iterable, Value *out) {
  if (isIterator(iterable)) {
    *out = iterable;
    return STATUS_OK;
  }
  push(iterable);
  if (!callMethod(vm.iterString, 0)) {
    return STATUS_ERROR;
  }
  *out = pop();
  return STATUS_OK;
}

Status valueIterNext(Value iterator, Value *out) {
  push(iterator);
  if (!callFunction(0)) {
    return STATUS_ERROR;
  }
  *out = pop();
  return STATUS_OK;
}

Status valueFastIter(Value iterable, Value *out) {
  if (isRange(iterable)) {
    iterable.type = VAL_RANGE_ITERATOR;
    *out = iterable;
    return STATUS_OK;
  }
  return valueIter(iterable, out);
}

Status valueFastIterNext(Value *iterator, Value *out) {
  if (isRangeIterator(*iterator)) {
    i32 step = iterator->as.range.step;
    if (step > 0 ? (iterator->extra.integer < iterator->as.range.stop)
                 : (iterator->extra.integer > iterator->as.range.stop)) {
      *out = valNumber(iterator->extra.integer);
      iterator->extra.integer += step;
    } else {
      *out = valStopIteration();
    }
    return STATUS_OK;
  }
  return valueIterNext(*iterator, out);
}

ubool valueGetItem(Value owner, Value key, Value *out) {
  if (isList(owner) && isNumber(key)) {
    ObjList *list = AS_LIST_UNSAFE(owner);
    size_t i = asIndex(key, list->length);
    *out = list->buffer[i];
    return STATUS_OK;
  }
  push(owner);
  push(key);
  if (!callMethod(vm.getitemString, 1)) {
    return STATUS_ERROR;
  }
  *out = pop();
  return STATUS_OK;
}

ubool valueSetItem(Value owner, Value key, Value value) {
  if (isList(owner) && isNumber(key)) {
    ObjList *list = AS_LIST_UNSAFE(owner);
    size_t i = asIndex(key, list->length);
    list->buffer[i] = value;
    return STATUS_OK;
  }
  push(owner);
  push(key);
  push(value);
  if (!callMethod(vm.setitemString, 2)) {
    return STATUS_ERROR;
  }
  pop(); /* method call result */
  return STATUS_OK;
}

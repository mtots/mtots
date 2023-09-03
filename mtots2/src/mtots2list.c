#include "mtots2list.h"

#include <stdlib.h>

#include "mtots1err.h"
#include "mtots2string.h"

struct List {
  Object object;
  size_t length;
  size_t capacity;
  Value *buffer;
  u32 hash;
  ubool frozen;
};

static void freeList(Object *object) {
  List *list = (List *)object;
  size_t i;
  for (i = 0; i < list->length; i++) {
    releaseValue(list->buffer[i]);
  }
  free(list->buffer);
}

Class LIST_CLASS = {
    "List",   /* name */
    0,        /* size */
    NULL,     /* constructor */
    freeList, /* desctructor */
};

void retainList(List *list) {
  retainObject((Object *)list);
}

void releaseList(List *list) {
  releaseObject((Object *)list);
}

Value listValue(List *list) {
  return objectValue((Object *)list);
}

ubool isList(Value value) {
  return isObject(value) && value.as.object->type == OBJECT_LIST;
}

List *asList(Value value) {
  if (!isList(value)) {
    panic("Expected List but got %s", getValueKindName(value));
  }
  return (List *)value.as.object;
}

void reprList(String *out, List *list) {
  size_t i;
  msputc('[', out);
  for (i = 0; i < list->length; i++) {
    if (i != 0) {
      msputs(", ", out);
    }
    reprValue(out, list->buffer[i]);
  }
  msputc(']', out);
}

ubool eqList(List *a, List *b) {
  size_t i;
  if (a == b) {
    return UTRUE;
  }
  if (a->length != b->length) {
    return UFALSE;
  }
  for (i = 0; i < a->length; i++) {
    if (!eqValue(a->buffer[i], b->buffer[i])) {
      return UFALSE;
    }
  }
  return UTRUE;
}

u32 hashList(List *list) {
  freezeList(list);
  return list->hash;
}

static u32 hashSequence(Value *values, size_t length) {
  /* FNV-1a as presented in the Crafting Interpreters book */
  size_t i;
  u32 hash = 2166136261u;
  for (i = 0; i < length; i++) {
    u32 itemhash = hashValue(values[i]);
    hash ^= (u8)(itemhash);
    hash *= 16777619;
    hash ^= (u8)(itemhash >> 8);
    hash *= 16777619;
    hash ^= (u8)(itemhash >> 16);
    hash *= 16777619;
    hash ^= (u8)(itemhash >> 24);
    hash *= 16777619;
  }
  return hash;
}

void freezeList(List *list) {
  if (list->frozen) {
    return;
  }
  list->frozen = UTRUE;
  list->capacity = list->length;
  list->buffer = (Value *)realloc(list->buffer, sizeof(Value) * list->capacity);
  list->hash = hashSequence(list->buffer, list->length);
}

size_t lenList(List *list) {
  return list->length;
}

NODISCARD List *newList(size_t length) {
  List *list = (List *)calloc(1, sizeof(List));
  list->object.type = OBJECT_LIST;
  if (length > 0) {
    size_t i;
    list->capacity = list->length = length;
    list->buffer = (Value *)malloc(length * sizeof(Value));
    for (i = 0; i < length; i++) {
      list->buffer[i].type = VALUE_NIL;
    }
  }
  return list;
}

void listResize(List *list, size_t newSize) {
  if (list->length < newSize) {
    if (newSize > list->capacity) {
      list->capacity = newSize < 8                    ? 8
                       : 2 * list->capacity < newSize ? newSize
                                                      : 2 * list->capacity;
      list->buffer = (Value *)realloc(list->buffer, list->capacity);
    }
    while (list->length < newSize) {
      list->buffer[list->length++].type = VALUE_NIL;
    }
  } else {
    do {
      releaseValue(list->buffer[--list->length]);
    } while (list->length > newSize);
  }
}

void listAppend(List *list, Value value) {
  listResize(list, list->length + 1);
  list->buffer[list->length - 1] = value;
  retainValue(value);
}

/** Pops the last element off the list.
 * As with most functions that return a Value,
 * the caller must release the returned Value. */
NODISCARD Value listPop(List *list) {
  if (list->length == 0) {
    panic("Pop from an empty List");
  }
  return list->buffer[--list->length];
}

/** NOTE: listLast does NOT create a new retain.
 * The list already holds a retain to each value in it, so
 * as long as the list is alive, and the entry not modified,
 * the returned value should be too */
Value listLast(List *list) {
  if (list->length == 0) {
    panic("Tried to get the last element of an empty List");
  }
  return list->buffer[list->length - 1];
}

/** NOTE: listGet does NOT create a new retain.
 * The list already holds a retain to each value in it, so
 * as long as the list is alive, and the entry not modified,
 * the returned value should be too */
Value listGet(List *list, size_t index) {
  if (index >= list->length) {
    panic("List index out of bounds (listGet, index = %lu, length = %lu)",
          (unsigned long)index,
          (unsigned long)list->length);
  }
  return list->buffer[index];
}

void listSet(List *list, size_t index, Value value) {
  if (list->frozen) {
    panic("Cannot modify a frozen List (listSet)");
  }
  if (index >= list->length) {
    panic("List index out of bounds (listSet, index = %lu, length = %lu)",
          (unsigned long)index,
          (unsigned long)list->length);
  }
  retainValue(value);
  releaseValue(list->buffer[index]);
  list->buffer[index] = value;
}

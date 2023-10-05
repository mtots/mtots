#include "mtots_class_frozenlist.h"

#include <stdlib.h>

#include "mtots_vm.h"

static Status implFrozenListInstantiate(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList;
  if (!newFrozenListFromIterable(args[0], &frozenList)) {
    return STATUS_ERROR;
  }
  *out = valFrozenList(frozenList);
  return STATUS_OK;
}

static CFunction funcFrozenListInstantiate = {implFrozenListInstantiate, "__call__", 1};

typedef struct ObjFrozenListIterator {
  ObjNative obj;
  ObjFrozenList *frozenList;
  size_t index;
} ObjFrozenListIterator;

static void blackenFrozenListIterator(ObjNative *n) {
  ObjFrozenListIterator *iter = (ObjFrozenListIterator *)n;
  markObject((Obj *)iter->frozenList);
}

static Status implFrozenListIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjFrozenListIterator *iter = (ObjFrozenListIterator *)AS_OBJ_UNSAFE(args[-1]);
  if (iter->index < iter->frozenList->length) {
    *out = iter->frozenList->buffer[iter->index++];
  } else {
    *out = valStopIteration();
  }
  return STATUS_OK;
}

static CFunction funcFrozenListIteratorCall = {
    implFrozenListIteratorCall,
    "__call__",
};

static NativeObjectDescriptor descriptorFrozenListIterator = {
    blackenFrozenListIterator,
    nopFree,
    sizeof(ObjFrozenListIterator),
    "FrozenListIterator",
};

static Status implFrozenListMul(i16 argCount, Value *args, Value *out) {
  Value *buffer;
  ObjFrozenList *frozenList = asFrozenList(args[-1]);
  size_t r, rep = asU32(args[0]);
  buffer = malloc(sizeof(Value) * frozenList->length * rep);
  for (r = 0; r < rep; r++) {
    size_t i;
    for (i = 0; i < frozenList->length; i++) {
      buffer[r * frozenList->length + i] = frozenList->buffer[i];
    }
  }
  *out = valFrozenList(copyFrozenList(buffer, frozenList->length * rep));
  free(buffer);
  return STATUS_OK;
}

static CFunction funcFrozenListMul = {implFrozenListMul, "__mul__", 1, 0};

static Status implFrozenListGetItem(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList = asFrozenList(args[-1]);
  i32 index;
  if (!isNumber(args[0])) {
    runtimeError("Expcted FrozenList index to be a number");
    return STATUS_ERROR;
  }
  index = asI32(args[0]);
  if (index < 0) {
    index += frozenList->length;
  }
  if (index < 0 || index >= frozenList->length) {
    runtimeError("FrozenList index out of bounds");
    return STATUS_ERROR;
  }
  *out = frozenList->buffer[index];
  return STATUS_OK;
}

static CFunction funcFrozenListGetItem = {implFrozenListGetItem, "__getitem__", 1};

#define NEW_FROZEN_LIST_CFUNC(index)                                              \
  static Status implFrozenListGet##index(i16 argCount, Value *args, Value *out) { \
    ObjFrozenList *frozenList = asFrozenList(args[-1]);                           \
    if (frozenList->length <= index) {                                            \
      runtimeError(                                                               \
          "FrozenList.get" #index "(): index out of bounds, length = %lu",        \
          (unsigned long)frozenList->length);                                     \
      return STATUS_ERROR;                                                        \
    }                                                                             \
    *out = frozenList->buffer[index];                                             \
    return STATUS_OK;                                                             \
  }                                                                               \
  static CFunction funcFrozenListGet##index = {implFrozenListGet##index, "get" #index};

NEW_FROZEN_LIST_CFUNC(0)
NEW_FROZEN_LIST_CFUNC(1)
NEW_FROZEN_LIST_CFUNC(2)
NEW_FROZEN_LIST_CFUNC(3)

#undef NEW_FROZEN_LIST_CFUNC

static Status implFrozenListIter(i16 argCount, Value *args, Value *out) {
  Value receiver = args[-1];
  ObjFrozenList *frozenList;
  ObjFrozenListIterator *iter;
  if (!isFrozenList(receiver)) {
    runtimeError("Expected frozenList as receiver to FrozenList.__iter__()");
    return STATUS_ERROR;
  }
  frozenList = asFrozenList(receiver);
  iter = NEW_NATIVE(ObjFrozenListIterator, &descriptorFrozenListIterator);
  iter->frozenList = frozenList;
  iter->index = 0;
  *out = valObjExplicit((Obj *)iter);
  return STATUS_OK;
}

static CFunction funcFrozenListIter = {implFrozenListIter, "__iter__", 0};

void initFrozenListClass(void) {
  {
    CFunction *methods[] = {
        &funcFrozenListMul,
        &funcFrozenListGetItem,
        &funcFrozenListGet0,
        &funcFrozenListGet1,
        &funcFrozenListGet2,
        &funcFrozenListGet3,
        &funcFrozenListIter,
        NULL,
    },
              *staticMethods[] = {
                  &funcFrozenListInstantiate,
                  NULL,
              };
    newBuiltinClass(
        "FrozenList",
        &vm.frozenListClass,
        methods,
        staticMethods);
  }

  {
    CFunction *methods[] = {
        &funcFrozenListIteratorCall,
        NULL,
    };
    newNativeClass(NULL, &descriptorFrozenListIterator, methods, NULL);
  }
}

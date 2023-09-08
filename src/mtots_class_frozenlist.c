#include "mtots_class_frozenlist.h"

#include <stdlib.h>

#include "mtots_vm.h"

static ubool implFrozenListInstantiate(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList;
  if (!newFrozenListFromIterable(args[0], &frozenList)) {
    return UFALSE;
  }
  *out = FROZEN_LIST_VAL(frozenList);
  return UTRUE;
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

static ubool implFrozenListIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjFrozenListIterator *iter = (ObjFrozenListIterator *)AS_OBJ(args[-1]);
  if (iter->index < iter->frozenList->length) {
    *out = iter->frozenList->buffer[iter->index++];
  } else {
    *out = STOP_ITERATION_VAL();
  }
  return UTRUE;
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

static ubool implFrozenListMul(i16 argCount, Value *args, Value *out) {
  Value *buffer;
  ObjFrozenList *frozenList = AS_FROZEN_LIST(args[-1]);
  size_t r, rep = asU32(args[0]);
  buffer = malloc(sizeof(Value) * frozenList->length * rep);
  for (r = 0; r < rep; r++) {
    size_t i;
    for (i = 0; i < frozenList->length; i++) {
      buffer[r * frozenList->length + i] = frozenList->buffer[i];
    }
  }
  *out = FROZEN_LIST_VAL(copyFrozenList(buffer, frozenList->length * rep));
  free(buffer);
  return UTRUE;
}

static CFunction funcFrozenListMul = {implFrozenListMul, "__mul__",
                                      1, 0, argsNumbers};

static ubool implFrozenListGetItem(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList = AS_FROZEN_LIST(args[-1]);
  i32 index;
  if (!IS_NUMBER(args[0])) {
    runtimeError("Expcted FrozenList index to be a number");
    return UFALSE;
  }
  index = asI32(args[0]);
  if (index < 0) {
    index += frozenList->length;
  }
  if (index < 0 || index >= frozenList->length) {
    runtimeError("FrozenList index out of bounds");
    return UFALSE;
  }
  *out = frozenList->buffer[index];
  return UTRUE;
}

static CFunction funcFrozenListGetItem = {implFrozenListGetItem, "__getitem__", 1};

#define NEW_FROZEN_LIST_CFUNC(index)                                             \
  static ubool implFrozenListGet##index(i16 argCount, Value *args, Value *out) { \
    ObjFrozenList *frozenList = AS_FROZEN_LIST(args[-1]);                        \
    if (frozenList->length <= index) {                                           \
      runtimeError(                                                              \
          "FrozenList.get" #index "(): index out of bounds, length = %lu",       \
          (unsigned long)frozenList->length);                                    \
      return UFALSE;                                                             \
    }                                                                            \
    *out = frozenList->buffer[index];                                            \
    return UTRUE;                                                                \
  }                                                                              \
  static CFunction funcFrozenListGet##index = {implFrozenListGet##index, "get" #index};

NEW_FROZEN_LIST_CFUNC(0)
NEW_FROZEN_LIST_CFUNC(1)
NEW_FROZEN_LIST_CFUNC(2)
NEW_FROZEN_LIST_CFUNC(3)

#undef NEW_FROZEN_LIST_CFUNC

static ubool implFrozenListIter(i16 argCount, Value *args, Value *out) {
  Value receiver = args[-1];
  ObjFrozenList *frozenList;
  ObjFrozenListIterator *iter;
  if (!IS_FROZEN_LIST(receiver)) {
    runtimeError("Expected frozenList as receiver to FrozenList.__iter__()");
    return UFALSE;
  }
  frozenList = AS_FROZEN_LIST(receiver);
  iter = NEW_NATIVE(ObjFrozenListIterator, &descriptorFrozenListIterator);
  iter->frozenList = frozenList;
  iter->index = 0;
  *out = OBJ_VAL_EXPLICIT((Obj *)iter);
  return UTRUE;
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
        TYPE_PATTERN_FROZEN_LIST,
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

#include "mtots_class_list.h"

#include <string.h>

#include "mtots_vm.h"

typedef struct Slice {
  Value *buffer;
  size_t length;
} Slice;

static Slice getSlice(Value value) {
  Slice slice;
  if (isFrozenList(value)) {
    ObjFrozenList *frozenList = (ObjFrozenList *)value.as.obj;
    slice.buffer = frozenList->buffer;
    slice.length = frozenList->length;
  } else {
    ObjList *list = asList(value);
    slice.buffer = list->buffer;
    slice.length = list->length;
  }
  return slice;
}

static Status implListStaticCall(i16 argc, Value *argv, Value *out) {
  ObjList *list;
  if (!newListFromIterable(argv[0], &list)) {
    return STATUS_ERROR;
  }
  *out = valList(list);
  return STATUS_OK;
}

static CFunction funcListStaticCall = {implListStaticCall, "__call__", 1};

static Status implFrozenListStaticCall(i16 argCount, Value *args, Value *out) {
  ObjFrozenList *frozenList;
  if (!newFrozenListFromIterable(args[0], &frozenList)) {
    return STATUS_ERROR;
  }
  *out = valFrozenList(frozenList);
  return STATUS_OK;
}

static CFunction funcFrozenListStaticCall = {implFrozenListStaticCall, "__call__", 1};

typedef struct ObjListIterator {
  ObjNative obj;
  Slice slice;
  Obj *list;
  size_t index;
} ObjListIterator;

static void blackenListIterator(ObjNative *n) {
  ObjListIterator *iter = (ObjListIterator *)n;
  markObject(iter->list);
}

static Status implListIteratorCall(i16 argc, Value *argv, Value *out) {
  ObjListIterator *iter = (ObjListIterator *)AS_OBJ_UNSAFE(argv[-1]);
  if (iter->index < iter->slice.length) {
    *out = iter->slice.buffer[iter->index++];
  } else {
    *out = valStopIteration();
  }
  return STATUS_OK;
}

static CFunction funcListIteratorCall = {
    implListIteratorCall,
    "__call__",
};

static NativeObjectDescriptor descriptorListIterator = {
    blackenListIterator,
    nopFree,
    sizeof(ObjListIterator),
    "ListIterator",
};

static Status implListClear(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  list->length = 0;
  return STATUS_OK;
}

static CFunction funcListClear = {implListClear, "clear"};

static Status implListAppend(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  listAppend(list, argv[0]);
  return STATUS_OK;
}

static CFunction funcListAppend = {implListAppend, "append", 1};

static Status implListExtend(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  Value iterable = argv[0], iterator;
  if (!valueFastIter(iterable, &iterator)) {
    return STATUS_ERROR;
  }
  push(iterator);
  for (;;) {
    Value item;
    if (!valueFastIterNext(&iterator, &item)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(item)) {
      break;
    }
    push(item);
    listAppend(list, item);
    pop(); /* item */
  }
  pop(); /* iterator */
  return STATUS_OK;
}

static CFunction funcListExtend = {implListExtend, "extend", 1};

static Status implListPop(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  size_t index = argc > 0 && !isNil(argv[0]) ? asIndex(argv[0], list->length) : list->length - 1;
  *out = list->buffer[index];
  for (; index + 1 < list->length; index++) {
    list->buffer[index] = list->buffer[index + 1];
  }
  list->length--;
  return STATUS_OK;
}

static CFunction funcListPop = {implListPop, "pop", 0, 1};

static Status implListInsert(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  size_t index = asIndex(argv[0], list->length + 1), i;
  Value value = argv[1];
  listAppend(list, valNil());
  for (i = list->length - 1; i > index; i--) {
    list->buffer[i] = list->buffer[i - 1];
  }
  list->buffer[index] = value;
  return STATUS_OK;
}

static CFunction funcListInsert = {implListInsert, "insert", 2};

static Status implListAdd(i16 argc, Value *argv, Value *out) {
  Slice lhs = getSlice(argv[-1]);
  Slice rhs = getSlice(argv[0]);
  ObjList *result = newList(lhs.length + rhs.length);
  memcpy(result->buffer, lhs.buffer, sizeof(Value) * lhs.length);
  memcpy(result->buffer + lhs.length, rhs.buffer, sizeof(Value) * rhs.length);
  if (isFrozenList(argv[-1])) {
    ubool gcFlag;
    locallyPauseGC(&gcFlag);
    *out = valFrozenList(copyFrozenList(result->buffer, result->length));
    locallyUnpauseGC(gcFlag);
  } else {
    *out = valList(result);
  }
  return STATUS_OK;
}

static CFunction funcListAdd = {implListAdd, "__add__", 1};

static Status implListMul(i16 argc, Value *argv, Value *out) {
  Slice slice = getSlice(argv[-1]);
  ObjList *result;
  size_t r, rep = asU32(argv[0]);
  result = newList(slice.length * rep);
  for (r = 0; r < rep; r++) {
    size_t i;
    for (i = 0; i < slice.length; i++) {
      result->buffer[r * slice.length + i] = slice.buffer[i];
    }
  }
  if (isFrozenList(argv[-1])) {
    ubool gcFlag;
    locallyPauseGC(&gcFlag);
    *out = valFrozenList(copyFrozenList(result->buffer, result->length));
    locallyUnpauseGC(gcFlag);
  } else {
    *out = valList(result);
  }
  return STATUS_OK;
}

static CFunction funcListMul = {implListMul, "__mul__", 1};

static Status implListGetItem(i16 argc, Value *argv, Value *out) {
  Slice slice = getSlice(argv[-1]);
  i32 index = asIndex(argv[0], slice.length);
  *out = slice.buffer[index];
  return STATUS_OK;
}

static CFunction funcListGetItem = {implListGetItem, "__getitem__", 1};

static Status implListSetItem(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  i32 index = asIndex(argv[0], list->length);
  list->buffer[index] = argv[1];
  return STATUS_OK;
}

static CFunction funcListSetItem = {implListSetItem, "__setitem__", 2};

static Status implListSlice(i16 argc, Value *argv, Value *out) {
  Slice slice = getSlice(argv[-1]);
  size_t start = isNil(argv[0]) ? 0 : asIndexLower(argv[0], slice.length);
  size_t end = isNil(argv[1]) ? slice.length : asIndexUpper(argv[1], slice.length);
  if (start > end) {
    runtimeError(
        "List.__slice__ start > end (%lu > %lu)",
        (unsigned long)start,
        (unsigned long)end);
    return STATUS_ERROR;
  }
  *out = valList(newListFromArray(slice.buffer + start, end - start));
  return STATUS_OK;
}

static CFunction funcListSlice = {implListSlice, "__slice__", 2};

static Status implListReverse(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  size_t i = 0, j = list->length;
  for (; i + 1 < j; i++, j--) {
    Value tmp = list->buffer[i];
    list->buffer[i] = list->buffer[j - 1];
    list->buffer[j - 1] = tmp;
  }
  return STATUS_OK;
}

static CFunction funcListReverse = {implListReverse, "reverse"};

static Status implListSort(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  return sortListWithKeyFunc(list, argc > 0 ? argv[0] : valNil());
}

static CFunction funcListSort = {implListSort, "sort", 0, 1};

static Status implListFlatten(i16 argc, Value *argv, Value *out) {
  ObjList *list = asList(argv[-1]);
  ObjList *result = newList(0);
  size_t i;
  push(valList(result));
  for (i = 0; i < list->length; i++) {
    Value iterator;
    if (!valueFastIter(list->buffer[i], &iterator)) {
      return STATUS_ERROR;
    }
    push(iterator);
    for (;;) {
      Value item;
      if (!valueFastIterNext(&iterator, &item)) {
        return STATUS_ERROR;
      }
      if (isStopIteration(item)) {
        break;
      }
      push(item);
      listAppend(result, item);
      pop(); /* item */
    }
    pop(); /* iterator */
  }
  *out = pop();
  return STATUS_OK;
}

static CFunction funcListFlatten = {implListFlatten, "flatten"};

static Status implListIter(i16 argc, Value *argv, Value *out) {
  Slice slice = getSlice(argv[-1]);
  ObjListIterator *iter;
  iter = NEW_NATIVE(ObjListIterator, &descriptorListIterator);
  iter->slice = slice;
  iter->list = argv[-1].as.obj;
  iter->index = 0;
  *out = valObjExplicit((Obj *)iter);
  return STATUS_OK;
}

static CFunction funcListIter = {implListIter, "__iter__", 0};

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

void initListClass(void) {
  CFunction *ListMethods[] = {
      &funcListClear,
      &funcListAppend,
      &funcListExtend,
      &funcListPop,
      &funcListInsert,
      &funcListAdd,
      &funcListMul,
      &funcListGetItem,
      &funcListSetItem,
      &funcListSlice,
      &funcListReverse,
      &funcListSort,
      &funcListFlatten,
      &funcListIter,
      NULL,
  };
  CFunction *ListStaticMethods[] = {
      &funcListStaticCall,
      NULL,
  };
  CFunction *FrozenListMethods[] = {
      &funcListAdd,
      &funcListMul,
      &funcListGetItem,
      &funcListSlice,
      &funcListIter,
      &funcFrozenListGet0,
      &funcFrozenListGet1,
      &funcFrozenListGet2,
      &funcFrozenListGet3,
      NULL,
  };
  CFunction *FrozenListStaticMethods[] = {
      &funcFrozenListStaticCall,
      NULL,
  };
  CFunction *ListIteratorMethods[] = {
      &funcListIteratorCall,
      NULL,
  };
  newBuiltinClass("List", &vm.listClass, ListMethods, ListStaticMethods);
  newBuiltinClass("FrozenList", &vm.frozenListClass, FrozenListMethods, FrozenListStaticMethods);
  newNativeClass(NULL, &descriptorListIterator, ListIteratorMethods, NULL);
}

#include "mtots_class_list.h"

#include <string.h>

#include "mtots_vm.h"

static Status implInstantiateList(i16 argCount, Value *args, Value *out) {
  ObjList *list;
  if (!newListFromIterable(args[0], &list)) {
    return STATUS_ERROR;
  }
  *out = LIST_VAL(list);
  return STATUS_OK;
}

static CFunction funcInstantiateList = {implInstantiateList, "__call__", 1};

typedef struct ObjListIterator {
  ObjNative obj;
  ObjList *list;
  size_t index;
} ObjListIterator;

static void blackenListIterator(ObjNative *n) {
  ObjListIterator *iter = (ObjListIterator *)n;
  markObject((Obj *)iter->list);
}

static Status implListIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjListIterator *iter = (ObjListIterator *)AS_OBJ_UNSAFE(args[-1]);
  if (iter->index < iter->list->length) {
    *out = iter->list->buffer[iter->index++];
  } else {
    *out = STOP_ITERATION_VAL();
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

static Status implListClear(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  list->length = 0;
  return STATUS_OK;
}

static CFunction funcListClear = {implListClear, "clear"};

static Status implListAppend(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  listAppend(list, args[0]);
  return STATUS_OK;
}

static CFunction funcListAppend = {implListAppend, "append", 1};

static Status implListExtend(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  Value iterable = args[0], iterator;
  if (!valueFastIter(iterable, &iterator)) {
    return STATUS_ERROR;
  }
  push(iterator);
  for (;;) {
    Value item;
    if (!valueFastIterNext(&iterator, &item)) {
      return STATUS_ERROR;
    }
    if (IS_STOP_ITERATION(item)) {
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

static Status implListPop(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  size_t index = argCount > 0 && !IS_NIL(args[0]) ? asIndex(args[0], list->length) : list->length - 1;
  *out = list->buffer[index];
  for (; index + 1 < list->length; index++) {
    list->buffer[index] = list->buffer[index + 1];
  }
  list->length--;
  return STATUS_OK;
}

static CFunction funcListPop = {implListPop, "pop", 0, 1};

static Status implListInsert(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  size_t index = asIndex(args[0], list->length + 1), i;
  Value value = args[1];
  listAppend(list, NIL_VAL());
  for (i = list->length - 1; i > index; i--) {
    list->buffer[i] = list->buffer[i - 1];
  }
  list->buffer[index] = value;
  return STATUS_OK;
}

static CFunction funcListInsert = {implListInsert, "insert", 2};

static Status implListAdd(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  ObjList *other = asList(args[0]);
  ObjList *result = newList(list->length + other->length);
  memcpy(result->buffer, list->buffer, sizeof(Value) * list->length);
  memcpy(result->buffer + list->length, other->buffer, sizeof(Value) * other->length);
  *out = LIST_VAL(result);
  return STATUS_OK;
}

static CFunction funcListAdd = {implListAdd, "__add__", 1};

static Status implListMul(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  ObjList *result;
  size_t r, rep = asU32(args[0]);
  result = newList(list->length * rep);
  for (r = 0; r < rep; r++) {
    size_t i;
    for (i = 0; i < list->length; i++) {
      result->buffer[r * list->length + i] = list->buffer[i];
    }
  }
  *out = LIST_VAL(result);
  return STATUS_OK;
}

static CFunction funcListMul = {implListMul, "__mul__", 1};

static Status implListGetItem(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  i32 index = asIndex(args[0], list->length);
  *out = list->buffer[index];
  return STATUS_OK;
}

static CFunction funcListGetItem = {implListGetItem, "__getitem__", 1};

static Status implListSetItem(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  i32 index = asIndex(args[0], list->length);
  list->buffer[index] = args[1];
  return STATUS_OK;
}

static CFunction funcListSetItem = {implListSetItem, "__setitem__", 2};

static Status implListSlice(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  size_t start = IS_NIL(args[0]) ? 0 : asIndexLower(args[0], list->length);
  size_t end = IS_NIL(args[1]) ? list->length : asIndexUpper(args[1], list->length);
  if (start > end) {
    runtimeError(
        "List.__slice__ start > end (%lu > %lu)",
        (unsigned long)start,
        (unsigned long)end);
    return STATUS_ERROR;
  }
  *out = LIST_VAL(newListFromArray(list->buffer + start, end - start));
  return STATUS_OK;
}

static CFunction funcListSlice = {implListSlice, "__slice__", 2};

static Status implListReverse(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  size_t i = 0, j = list->length;
  for (; i + 1 < j; i++, j--) {
    Value tmp = list->buffer[i];
    list->buffer[i] = list->buffer[j - 1];
    list->buffer[j - 1] = tmp;
  }
  return STATUS_OK;
}

static CFunction funcListReverse = {implListReverse, "reverse"};

static Status implListSort(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  return sortListWithKeyFunc(list, argCount > 0 ? args[0] : NIL_VAL());
}

static CFunction funcListSort = {implListSort, "sort", 0, 1};

static Status implListFlatten(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  ObjList *result = newList(0);
  size_t i;
  push(LIST_VAL(result));
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
      if (IS_STOP_ITERATION(item)) {
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

static Status implListIter(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[-1]);
  ObjListIterator *iter;
  iter = NEW_NATIVE(ObjListIterator, &descriptorListIterator);
  iter->list = list;
  iter->index = 0;
  *out = OBJ_VAL_EXPLICIT((Obj *)iter);
  return STATUS_OK;
}

static CFunction funcListIter = {implListIter, "__iter__", 0};

void initListClass(void) {
  {
    CFunction *methods[] = {
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
    },
              *staticMethods[] = {
                  &funcInstantiateList,
                  NULL,
              };
    newBuiltinClass("List", &vm.listClass, methods, staticMethods);
  }

  {
    CFunction *methods[] = {
        &funcListIteratorCall,
        NULL,
    };
    newNativeClass(NULL, &descriptorListIterator, methods, NULL);
  }
}

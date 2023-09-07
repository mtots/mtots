#include "mtots_class_list.h"

#include <string.h>

#include "mtots_vm.h"

static ubool implInstantiateList(i16 argCount, Value *args, Value *out) {
  ObjList *list;
  if (!newListFromIterable(args[0], &list)) {
    return UFALSE;
  }
  *out = LIST_VAL(list);
  return UTRUE;
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

static ubool implListIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjListIterator *iter = (ObjListIterator *)AS_OBJ(args[-1]);
  if (iter->index < iter->list->length) {
    *out = iter->list->buffer[iter->index++];
  } else {
    *out = STOP_ITERATION_VAL();
  }
  return UTRUE;
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

static ubool implListClear(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  list->length = 0;
  return UTRUE;
}

static CFunction funcListClear = {implListClear, "clear"};

static ubool implListAppend(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  listAppend(list, args[0]);
  return UTRUE;
}

static CFunction funcListAppend = {implListAppend, "append", 1};

static ubool implListExtend(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  Value iterable = args[0], iterator;
  if (!valueFastIter(iterable, &iterator)) {
    return UFALSE;
  }
  push(iterator);
  for (;;) {
    Value item;
    if (!valueFastIterNext(&iterator, &item)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(item)) {
      break;
    }
    push(item);
    listAppend(list, item);
    pop(); /* item */
  }
  pop(); /* iterator */
  return UTRUE;
}

static CFunction funcListExtend = {implListExtend, "extend", 1};

static ubool implListPop(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  size_t index = argCount > 0 && !IS_NIL(args[0]) ? AS_INDEX(args[0], list->length) : list->length - 1;
  *out = list->buffer[index];
  for (; index + 1 < list->length; index++) {
    list->buffer[index] = list->buffer[index + 1];
  }
  list->length--;
  return UTRUE;
}

static TypePattern argsListPop[] = {
    {TYPE_PATTERN_NUMBER_OR_NIL},
};

static CFunction funcListPop = {implListPop, "pop", 0, 1, argsListPop};

static ubool implListInsert(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  size_t index = AS_INDEX(args[0], list->length + 1), i;
  Value value = args[1];
  listAppend(list, NIL_VAL());
  for (i = list->length - 1; i > index; i--) {
    list->buffer[i] = list->buffer[i - 1];
  }
  list->buffer[index] = value;
  return UTRUE;
}

static TypePattern argsListInsert[] = {
    {TYPE_PATTERN_NUMBER},
    {TYPE_PATTERN_ANY},
};

static CFunction funcListInsert = {implListInsert, "insert", 2, 0, argsListInsert};

static ubool implListAdd(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  ObjList *other = AS_LIST(args[0]);
  ObjList *result = newList(list->length + other->length);
  memcpy(result->buffer, list->buffer, sizeof(Value) * list->length);
  memcpy(result->buffer + list->length, other->buffer, sizeof(Value) * other->length);
  *out = LIST_VAL(result);
  return UTRUE;
}

static TypePattern argsListAdd[] = {
    {TYPE_PATTERN_LIST},
};

static CFunction funcListAdd = {
    implListAdd,
    "__add__",
    1,
    0,
    argsListAdd,
};

static ubool implListMul(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  ObjList *result;
  size_t r, rep = AS_U32(args[0]);
  result = newList(list->length * rep);
  for (r = 0; r < rep; r++) {
    size_t i;
    for (i = 0; i < list->length; i++) {
      result->buffer[r * list->length + i] = list->buffer[i];
    }
  }
  *out = LIST_VAL(result);
  return UTRUE;
}

static CFunction funcListMul = {
    implListMul,
    "__mul__",
    1,
    0,
    argsNumbers,
};

static ubool implListGetItem(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  i32 index = AS_INDEX(args[0], list->length);
  *out = list->buffer[index];
  return UTRUE;
}

static CFunction funcListGetItem = {
    implListGetItem,
    "__getitem__",
    1,
    0,
    argsNumbers,
};

static ubool implListSetItem(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  i32 index = AS_INDEX(args[0], list->length);
  list->buffer[index] = args[1];
  return UTRUE;
}

static TypePattern argsListSetItem[] = {
    {TYPE_PATTERN_NUMBER},
    {TYPE_PATTERN_ANY},
};

static CFunction funcListSetItem = {
    implListSetItem, "__setitem__", 2, 0, argsListSetItem};

static ubool implListSlice(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  size_t start = IS_NIL(args[0]) ? 0 : AS_INDEX_LOWER(args[0], list->length);
  size_t end = IS_NIL(args[1]) ? list->length : AS_INDEX_UPPER(args[1], list->length);
  if (start > end) {
    runtimeError(
        "List.__slice__ start > end (%lu > %lu)",
        (unsigned long)start,
        (unsigned long)end);
    return UFALSE;
  }
  *out = LIST_VAL(newListFromArray(list->buffer + start, end - start));
  return UTRUE;
}

static TypePattern argsListSlice[] = {
    {TYPE_PATTERN_NUMBER_OR_NIL},
    {TYPE_PATTERN_NUMBER_OR_NIL},
};

static CFunction funcListSlice = {
    implListSlice, "__slice__", 2, 0, argsListSlice};

static ubool implListReverse(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  size_t i = 0, j = list->length;
  for (; i + 1 < j; i++, j--) {
    Value tmp = list->buffer[i];
    list->buffer[i] = list->buffer[j - 1];
    list->buffer[j - 1] = tmp;
  }
  return UTRUE;
}

static CFunction funcListReverse = {implListReverse, "reverse"};

static ubool implListSort(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  return sortListWithKeyFunc(list, argCount > 0 ? args[0] : NIL_VAL());
}

static CFunction funcListSort = {implListSort, "sort", 0, 1};

static ubool implListFlatten(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  ObjList *result = newList(0);
  size_t i;
  push(LIST_VAL(result));
  for (i = 0; i < list->length; i++) {
    Value iterator;
    if (!valueFastIter(list->buffer[i], &iterator)) {
      return UFALSE;
    }
    push(iterator);
    for (;;) {
      Value item;
      if (!valueFastIterNext(&iterator, &item)) {
        return UFALSE;
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
  return UTRUE;
}

static CFunction funcListFlatten = {implListFlatten, "flatten"};

static ubool implListIter(i16 argCount, Value *args, Value *out) {
  ObjList *list = AS_LIST(args[-1]);
  ObjListIterator *iter;
  iter = NEW_NATIVE(ObjListIterator, &descriptorListIterator);
  iter->list = list;
  iter->index = 0;
  *out = OBJ_VAL_EXPLICIT((Obj *)iter);
  return UTRUE;
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
    newBuiltinClass("List", &vm.listClass, TYPE_PATTERN_LIST, methods, staticMethods);
  }

  {
    CFunction *methods[] = {
        &funcListIteratorCall,
        NULL,
    };
    newNativeClass(NULL, &descriptorListIterator, methods, NULL);
  }
}

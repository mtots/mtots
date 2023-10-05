#include "mtots_class_dict.h"

#include "mtots_vm.h"

static Status implInstantiateDict(i16 argCount, Value *args, Value *out) {
  ObjDict *dict;
  if (!newDictFromMap(args[0], &dict)) {
    return STATUS_ERROR;
  }
  *out = valDict(dict);
  return STATUS_OK;
}

static CFunction funcInstantiateDict = {implInstantiateDict, "__call__", 1};

typedef struct ObjDictIterator {
  ObjNative obj;
  ObjDict *dict;
  MapIterator di;
} ObjDictIterator;

static void blackenDictIterator(ObjNative *n) {
  ObjDictIterator *di = (ObjDictIterator *)n;
  markObject((Obj *)(di->dict));
}

static Status implDictIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjDictIterator *iter = (ObjDictIterator *)AS_OBJ_UNSAFE(args[-1]);
  if (mapIteratorNextKey(&iter->di, out)) {
    return STATUS_OK;
  }
  *out = valStopIteration();
  return STATUS_OK;
}

static CFunction funcDictIteratorCall = {
    implDictIteratorCall,
    "__call__",
};

static NativeObjectDescriptor descriptorDictIterator = {
    blackenDictIterator,
    nopFree,
    sizeof(ObjDictIterator),
    "DictIterator",
};

static Status implDictGetOrNil(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  if (!mapGet(&dict->map, args[0], out)) {
    *out = valNil();
  }
  return STATUS_OK;
}

static CFunction funcDictGetOrNil = {implDictGetOrNil, "get", 1};

static Status implDictGet(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  if (!mapGet(&dict->map, args[0], out)) {
    *out = args[1];
  }
  return STATUS_OK;
}

static CFunction funcDictGet = {implDictGet, "get", 2};

static Status implDictGetItem(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  if (!mapGet(&dict->map, args[0], out)) {
    runtimeError("Key not found in dict");
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcDictGetItem = {implDictGetItem, "__getitem__", 1};

static Status implDictSetItem(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  *out = valBool(mapSet(&dict->map, args[0], args[1]));
  return STATUS_OK;
}

static CFunction funcDictSetItem = {implDictSetItem, "__setitem__", 2};

static Status implDictDelete(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  *out = valBool(mapDelete(&dict->map, args[0]));
  return STATUS_OK;
}

static CFunction funcDictDelete = {implDictDelete, "delete", 1};

static Status implDictContains(i16 argCount, Value *args, Value *out) {
  Value dummy;
  ObjDict *dict = asDict(args[-1]);
  *out = valBool(mapGet(&dict->map, args[0], &dummy));
  return STATUS_OK;
}

static CFunction funcDictContains = {implDictContains, "__contains__", 1};

static Status implDictIter(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  ObjDictIterator *iter;
  iter = NEW_NATIVE(ObjDictIterator, &descriptorDictIterator);
  iter->dict = dict;
  initMapIterator(&iter->di, &dict->map);
  *out = valObjExplicit((Obj *)iter);
  return STATUS_OK;
}

static CFunction funcDictIter = {implDictIter, "__iter__", 0};

/**
 * "Reverse Get" or inverse lookup
 *
 * Find a key with the given value
 * If more than one entry has the same value, this function
 * will return the first matching key that would've been returned
 * in an iteration of the mapionary
 *
 * Implementation is a slow linear search, but a method like this
 * is still handy sometimes.
 */
static Status implDictRget(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  Value value = args[0];
  MapIterator di;
  MapEntry *entry;

  initMapIterator(&di, &dict->map);
  while (mapIteratorNext(&di, &entry)) {
    if (valuesEqual(entry->value, value)) {
      *out = entry->key;
      return STATUS_OK;
    }
  }

  if (argCount > 1) {
    /* If the optional second argument is provided, we return that
     * when a matching entry is not found */
    *out = args[1];
    return STATUS_OK;
  }
  /* If no entry is found, and no optional argument is provided
   * we throw an error */
  runtimeError("No entry with given value found in Dict");
  return STATUS_ERROR;
}

static CFunction funcDictRget = {implDictRget, "rget", 1, 2};

static Status implDictFreeze(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = asDict(args[-1]);
  ObjFrozenDict *fdict = newFrozenDict(&dict->map);
  *out = valFrozenDict(fdict);
  return STATUS_OK;
}

static CFunction funcDictFreeze = {implDictFreeze, "freeze", 0};

static Status implDictStaticFromPairs(i16 argCount, Value *args, Value *out) {
  ObjDict *dict;
  if (!newDictFromPairs(args[0], &dict)) {
    return STATUS_ERROR;
  }
  *out = valDict(dict);
  return STATUS_OK;
}

static CFunction funcDictStaticFromPairs = {
    implDictStaticFromPairs, "fromPairs", 1};

void initDictClass(void) {
  {
    CFunction *methods[] = {
        &funcDictGetOrNil,
        &funcDictGet,
        &funcDictGetItem,
        &funcDictSetItem,
        &funcDictDelete,
        &funcDictContains,
        &funcDictIter,
        &funcDictRget,
        &funcDictFreeze,
        NULL,
    },
              *staticMethods[] = {
                  &funcInstantiateDict,
                  &funcDictStaticFromPairs,
                  NULL,
              };
    newBuiltinClass("Dict", &vm.dictClass, methods, staticMethods);
  }

  {
    CFunction *methods[] = {
        &funcDictIteratorCall,
        NULL,
    };
    newNativeClass(NULL, &descriptorDictIterator, methods, NULL);
  }
}

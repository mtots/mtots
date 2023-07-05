#include "mtots_vm.h"

static ubool implInstantiateDict(i16 argCount, Value *args, Value *out) {
  ObjDict *dict;
  if (!newDictFromMap(args[0], &dict)) {
    return UFALSE;
  }
  *out = DICT_VAL(dict);
  return UTRUE;
}

static CFunction funcInstantiateDict = { implInstantiateDict, "__call__", 1 };

typedef struct ObjDictIterator {
  ObjNative obj;
  ObjDict *dict;
  MapIterator di;
} ObjDictIterator;

static void blackenDictIterator(ObjNative *n) {
  ObjDictIterator *di = (ObjDictIterator*)n;
  markObject((Obj*)(di->dict));
}

static ubool implDictIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjDictIterator *iter = (ObjDictIterator*)AS_OBJ(args[-1]);
  if (mapIteratorNextKey(&iter->di, out)) {
    return UTRUE;
  }
  *out = STOP_ITERATION_VAL();
  return UTRUE;
}

static CFunction funcDictIteratorCall = {
  implDictIteratorCall, "__call__",
};

static NativeObjectDescriptor descriptorDictIterator = {
  blackenDictIterator, nopFree,
  sizeof(ObjDictIterator),
  "DictIterator",
};

static ubool implDictGetOrNil(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  if (!mapGet(&dict->map, args[0], out)) {
    *out = NIL_VAL();
  }
  return UTRUE;
}

static CFunction funcDictGetOrNil = { implDictGetOrNil, "get", 1 };

static ubool implDictGet(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  if (!mapGet(&dict->map, args[0], out)) {
    *out = args[1];
  }
  return UTRUE;
}

static CFunction funcDictGet = { implDictGet, "get", 2 };

static ubool implDictGetItem(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  if (!mapGet(&dict->map, args[0], out)) {
    runtimeError("Key not found in dict");
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcDictGetItem = { implDictGetItem, "__getitem__", 1 };

static ubool implDictSetItem(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  *out = BOOL_VAL(mapSet(&dict->map, args[0], args[1]));
  return UTRUE;
}

static CFunction funcDictSetItem = { implDictSetItem, "__setitem__", 2 };

static ubool implDictDelete(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  *out = BOOL_VAL(mapDelete(&dict->map, args[0]));
  return UTRUE;
}

static CFunction funcDictDelete = { implDictDelete, "delete", 1 };

static ubool implDictContains(i16 argCount, Value *args, Value *out) {
  Value dummy;
  ObjDict *dict = AS_DICT(args[-1]);
  *out = BOOL_VAL(mapGet(&dict->map, args[0], &dummy));
  return UTRUE;
}

static CFunction funcDictContains = { implDictContains, "__contains__", 1 };

static ubool implDictIter(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  ObjDictIterator *iter;
  iter = NEW_NATIVE(ObjDictIterator, &descriptorDictIterator);
  iter->dict = dict;
  initMapIterator(&iter->di, &dict->map);
  *out = OBJ_VAL_EXPLICIT((Obj*)iter);
  return UTRUE;
}

static CFunction funcDictIter = { implDictIter, "__iter__", 0 };

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
static ubool implDictRget(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  Value value = args[0];
  MapIterator di;
  MapEntry *entry;

  initMapIterator(&di, &dict->map);
  while (mapIteratorNext(&di, &entry)) {
    if (valuesEqual(entry->value, value)) {
      *out = entry->key;
      return UTRUE;
    }
  }

  if (argCount > 1) {
    /* If the optional second argument is provided, we return that
     * when a matching entry is not found */
    *out = args[1];
    return UTRUE;
  }
  /* If no entry is found, and no optional argument is provided
   * we throw an error */
  runtimeError("No entry with given value found in Dict");
  return UFALSE;
}

static CFunction funcDictRget = { implDictRget, "rget", 1, 2 };

static ubool implDictFreeze(i16 argCount, Value *args, Value *out) {
  ObjDict *dict = AS_DICT(args[-1]);
  ObjFrozenDict *fdict = newFrozenDict(&dict->map);
  *out = FROZEN_DICT_VAL(fdict);
  return UTRUE;
}

static CFunction funcDictFreeze = { implDictFreeze, "freeze", 0 };

static ubool implDictStaticFromPairs(i16 argCount, Value *args, Value *out) {
  ObjDict *dict;
  if (!newDictFromPairs(args[0], &dict)) {
    return UFALSE;
  }
  *out = DICT_VAL(dict);
  return UTRUE;
}

static CFunction funcDictStaticFromPairs = {
  implDictStaticFromPairs, "fromPairs", 1
};

void initDictClass() {
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
    }, *staticMethods[] = {
      &funcInstantiateDict,
      &funcDictStaticFromPairs,
      NULL,
    };
    newBuiltinClass("Dict", &vm.dictClass, TYPE_PATTERN_DICT, methods, staticMethods);
  }

  {
    CFunction *methods[] = {
      &funcDictIteratorCall,
      NULL,
    };
    newNativeClass(NULL, &descriptorDictIterator, methods, NULL);
  }
}

#include "mtots_class_dict.h"

#include "mtots_vm.h"

static Map *getMap(Value value) {
  if (isFrozenDict(value)) {
    return &((ObjFrozenDict *)value.as.obj)->map;
  }
  return &asDict(value)->map;
}

static Status implDictStaticCall(i16 argc, Value *argv, Value *out) {
  ObjDict *dict;
  if (!newDictFromMap(argv[0], &dict)) {
    return STATUS_ERROR;
  }
  *out = valDict(dict);
  return STATUS_OK;
}

static CFunction funcDictStaticCall = {implDictStaticCall, "__call__", 1};

typedef struct ObjDictIterator {
  ObjNative obj;
  Obj *dict;
  MapIterator di;
} ObjDictIterator;

static void blackenDictIterator(ObjNative *n) {
  ObjDictIterator *di = (ObjDictIterator *)n;
  markObject(di->dict);
}

static Status implDictIteratorCall(i16 argc, Value *argv, Value *out) {
  ObjDictIterator *iter = (ObjDictIterator *)AS_OBJ_UNSAFE(argv[-1]);
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

static Status implDictGetOrNil(i16 argc, Value *argv, Value *out) {
  Map *map = getMap(argv[-1]);
  if (!mapGet(map, argv[0], out)) {
    *out = valNil();
  }
  return STATUS_OK;
}

static CFunction funcDictGetOrNil = {implDictGetOrNil, "getOrNil", 1};

static Status implDictGet(i16 argc, Value *argv, Value *out) {
  Map *map = getMap(argv[-1]);
  if (!mapGet(map, argv[0], out)) {
    *out = argv[1];
  }
  return STATUS_OK;
}

static CFunction funcDictGet = {implDictGet, "get", 2};

static Status implDictGetItem(i16 argc, Value *argv, Value *out) {
  Map *map = getMap(argv[-1]);
  if (!mapGet(map, argv[0], out)) {
    runtimeError("Key not found in dict");
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcDictGetItem = {implDictGetItem, "__getitem__", 1};

static Status implDictSetItem(i16 argc, Value *argv, Value *out) {
  ObjDict *dict = asDict(argv[-1]);
  *out = valBool(mapSet(&dict->map, argv[0], argv[1]));
  return STATUS_OK;
}

static CFunction funcDictSetItem = {implDictSetItem, "__setitem__", 2};

static Status implDictDelete(i16 argc, Value *argv, Value *out) {
  ObjDict *dict = asDict(argv[-1]);
  *out = valBool(mapDelete(&dict->map, argv[0]));
  return STATUS_OK;
}

static CFunction funcDictDelete = {implDictDelete, "delete", 1};

static Status implDictContains(i16 argc, Value *argv, Value *out) {
  Map *map = getMap(argv[-1]);
  *out = valBool(mapContainsKey(map, argv[0]));
  return STATUS_OK;
}

static CFunction funcDictContains = {implDictContains, "__contains__", 1};

static Status implDictIter(i16 argc, Value *argv, Value *out) {
  Map *map = getMap(argv[-1]);
  ObjDictIterator *iter;
  iter = NEW_NATIVE(ObjDictIterator, &descriptorDictIterator);
  iter->dict = argv[-1].as.obj;
  initMapIterator(&iter->di, map);
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
static Status implDictRget(i16 argc, Value *argv, Value *out) {
  Map *map = getMap(argv[-1]);
  Value value = argv[0];
  MapIterator di;
  MapEntry *entry;

  initMapIterator(&di, map);
  while (mapIteratorNext(&di, &entry)) {
    if (valuesEqual(entry->value, value)) {
      *out = entry->key;
      return STATUS_OK;
    }
  }

  if (argc > 1) {
    /* If the optional second argument is provided, we return that
     * when a matching entry is not found */
    *out = argv[1];
    return STATUS_OK;
  }
  /* If no entry is found, and no optional argument is provided
   * we throw an error */
  runtimeError("No entry with given value found in Dict");
  return STATUS_ERROR;
}

static CFunction funcDictRget = {implDictRget, "rget", 1, 2};

static Status implDictFreeze(i16 argc, Value *argv, Value *out) {
  ObjDict *dict = asDict(argv[-1]);
  ObjFrozenDict *fdict = newFrozenDict(&dict->map);
  *out = valFrozenDict(fdict);
  return STATUS_OK;
}

static CFunction funcDictFreeze = {implDictFreeze, "freeze", 0};

static Status implDictStaticFromPairs(i16 argc, Value *argv, Value *out) {
  ObjDict *dict;
  if (!newDictFromPairs(argv[0], &dict)) {
    return STATUS_ERROR;
  }
  *out = valDict(dict);
  return STATUS_OK;
}

static CFunction funcDictStaticFromPairs = {implDictStaticFromPairs, "fromPairs", 1};

void initDictClass(void) {
  CFunction *DictStaticMethods[] = {
      &funcDictStaticCall,
      &funcDictStaticFromPairs,
      NULL,
  };
  CFunction *DictMethods[] = {
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
  };
  CFunction *FrozenDictMethods[] = {
      &funcDictGetItem,
      &funcDictContains,
      &funcDictIter,
      &funcDictRget,
      NULL,
  };
  CFunction *DictIteratorMethods[] = {
      &funcDictIteratorCall,
      NULL,
  };
  newBuiltinClass("Dict", &vm.dictClass, DictMethods, DictStaticMethods);
  newBuiltinClass("FrozenDict", &vm.frozenDictClass, FrozenDictMethods, NULL);
  newNativeClass(NULL, &descriptorDictIterator, DictIteratorMethods, NULL);
}

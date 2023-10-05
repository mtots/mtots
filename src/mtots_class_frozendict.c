#include "mtots_class_frozendict.h"

#include "mtots_vm.h"

typedef struct ObjFrozenDictIterator {
  ObjNative obj;
  ObjFrozenDict *dict;
  MapIterator di;
} ObjFrozenDictIterator;

static void blackenFrozenDictIterator(ObjNative *n) {
  ObjFrozenDictIterator *di = (ObjFrozenDictIterator *)n;
  markObject((Obj *)(di->dict));
}

static Status implFrozenDictIteratorCall(i16 argCount, Value *args, Value *out) {
  ObjFrozenDictIterator *iter = (ObjFrozenDictIterator *)AS_OBJ_UNSAFE(args[-1]);
  if (mapIteratorNextKey(&iter->di, out)) {
    return STATUS_OK;
  }
  *out = valStopIteration();
  return STATUS_OK;
}

static CFunction funcFrozenDictIteratorCall = {implFrozenDictIteratorCall, "__call__"};

static NativeObjectDescriptor descriptorFrozenDictIterator = {
    blackenFrozenDictIterator,
    nopFree,
    sizeof(ObjFrozenDictIterator),
    "FrozenDictIterator",
};

static Status implFrozenDictGetItem(i16 argCount, Value *args, Value *out) {
  ObjFrozenDict *dict = asFrozenDict(args[-1]);
  if (!mapGet(&dict->map, args[0], out)) {
    runtimeError("Key not found in dict");
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcFrozenDictGetItem = {implFrozenDictGetItem, "__getitem__", 1};

static Status implFrozenDictContains(i16 argCount, Value *args, Value *out) {
  Value dummy;
  ObjFrozenDict *dict = asFrozenDict(args[-1]);
  *out = valBool(mapGet(&dict->map, args[0], &dummy));
  return STATUS_OK;
}

static CFunction funcFrozenDictContains = {implFrozenDictContains, "__contains__", 1};

static Status implFrozenDictIter(i16 argCount, Value *args, Value *out) {
  ObjFrozenDict *dict = asFrozenDict(args[-1]);
  ObjFrozenDictIterator *iter = NEW_NATIVE(ObjFrozenDictIterator, &descriptorFrozenDictIterator);
  iter->dict = dict;
  initMapIterator(&iter->di, &dict->map);
  *out = valObjExplicit((Obj *)iter);
  return STATUS_OK;
}

static CFunction funcFrozenDictIter = {implFrozenDictIter, "__iter__", 0};

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
static Status implFrozenDictRget(i16 argCount, Value *args, Value *out) {
  ObjFrozenDict *dict = asFrozenDict(args[-1]);
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
  runtimeError("No entry with given value found in FrozenDict");
  return STATUS_ERROR;
}

static CFunction funcFrozenDictRget = {implFrozenDictRget, "rget", 1, 2};

void initFrozenDictClass(void) {
  {
    CFunction *methods[] = {
        &funcFrozenDictGetItem,
        &funcFrozenDictContains,
        &funcFrozenDictIter,
        &funcFrozenDictRget,
        NULL,
    };
    newBuiltinClass(
        "FrozenDict", &vm.frozenDictClass, methods, NULL);
  }

  {
    CFunction *methods[] = {
        &funcFrozenDictIteratorCall,
        NULL,
    };
    newNativeClass(NULL, &descriptorFrozenDictIterator, methods, NULL);
  }
}

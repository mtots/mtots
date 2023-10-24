#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#define ALLOCATE_OBJ(type, objectType) \
  (type *)allocateObject(sizeof(type), objectType, NULL)

/* should-be-inline */ ubool isObjType(Value value, ObjType type) {
  return isObj(value) && AS_OBJ_UNSAFE(value)->type == type;
}

/* Returns the value's native object descriptor if the value is a
 * native value. Otherwise returns NULL. */
NativeObjectDescriptor *getNativeObjectDescriptor(Value value) {
  if (isNativeObj(value)) {
    return AS_NATIVE_UNSAFE(value)->descriptor;
  }
  return NULL;
}

void nopBlacken(ObjNative *n) {}
void nopFree(ObjNative *n) {}

static Obj *allocateObject(size_t size, ObjType type, const char *typeName) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = UFALSE;
  object->next = vm.memory.objects;
  vm.memory.objects = object;
  if (vm.enableMallocFreeLogs) {
    eprintln(
        "DEBUG: allocate Object %s at %p",
        typeName ? typeName : getObjectTypeName(type),
        (void *)object);
  }
  return object;
}

ubool isModule(Value value) {
  return isInstance(value) && AS_INSTANCE_UNSAFE(value)->klass->isModuleClass;
}

ObjClass *asClass(Value value) {
  if (!isClass(value)) {
    panic("Expected Class but got %s", getKindName(value));
  }
  return (ObjClass *)value.as.obj;
}

ObjModule *asModule(Value value) {
  if (!isModule(value)) {
    panic("Expected Module but got %s", getKindName(value));
  }
  return (ObjModule *)value.as.obj;
}

ObjBuffer *asBuffer(Value value) {
  if (!isBuffer(value)) {
    panic("Expected Buffer but got %s", getKindName(value));
  }
  return (ObjBuffer *)value.as.obj;
}

ObjList *asList(Value value) {
  if (!isList(value)) {
    panic("Expected List but got %s", getKindName(value));
  }
  return (ObjList *)value.as.obj;
}

ObjFrozenList *asFrozenList(Value value) {
  if (!isFrozenList(value)) {
    panic("Expected FrozenList but got %s", getKindName(value));
  }
  return (ObjFrozenList *)value.as.obj;
}

ObjDict *asDict(Value value) {
  if (!isDict(value)) {
    panic("Expected Dict but got %s", getKindName(value));
  }
  return (ObjDict *)value.as.obj;
}

ObjFrozenDict *asFrozenDict(Value value) {
  if (!isFrozenDict(value)) {
    panic("Expected FrozenDict but got %s", getKindName(value));
  }
  return (ObjFrozenDict *)value.as.obj;
}

ObjModule *newModule(String *name, ubool includeGlobals) {
  ObjClass *klass;
  ObjModule *instance;

  klass = newClass(name);
  klass->isModuleClass = UTRUE;

  push(valClass(klass));
  instance = newInstance(klass);
  pop(); /* klass */

  if (includeGlobals) {
    push(valInstance(instance));
    mapAddAll(&vm.globals, &instance->fields);
    pop(); /* instance */
  }

  return instance;
}

ObjModule *newModuleFromCString(const char *name, ubool includeGlobals) {
  String *nameStr;
  ObjModule *instance;

  nameStr = internCString(name);
  push(valString(nameStr));
  instance = newModule(nameStr, includeGlobals);
  pop(); /* nameStr */

  return instance;
}

void moduleAddFunctions(ObjModule *module, CFunction **functions) {
  CFunction **function;
  for (function = functions; *function; function++) {
    mapSetN(&module->fields, (*function)->name, valCFunction(*function));
  }
}

/**
 * Retain a value by attaching it to the given module's retain list.
 */
void moduleRetain(ObjModule *module, Value value) {
  push(value); /* Make sure to keep the value alive */

  if (!module->retainList) {
    module->retainList = newList(0);
  }
  listAppend(module->retainList, value);

  pop(); /* value */
}

void moduleRelease(ObjModule *module, Value value) {
  size_t i;

  if (module->retainList) {
    ObjList *list = module->retainList;
    for (i = 0; i < list->length; i++) {
      if (valuesIs(list->buffer[i], value)) {
        list->buffer[i] = list->buffer[--list->length];
        return;
      }
    }
  }
}

/**
 * Create a new String from a c-string, and attach it to the
 * given module's retain list to retain the string.
 */
String *moduleRetainCString(ObjModule *module, const char *value) {
  String *string = internCString(value);
  moduleRetain(module, valString(string));
  return string;
}

ObjClass *newClass(String *name) {
  ObjClass *klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name;
  initMap(&klass->methods);
  initMap(&klass->staticMethods);
  initMap(&klass->fieldGetters);
  initMap(&klass->fieldSetters);
  klass->isModuleClass = UFALSE;
  klass->isBuiltinClass = UFALSE;
  klass->descriptor = NULL;
  klass->instantiate = NULL;
  klass->call = NULL;
  klass->getattr = NULL;
  klass->setattr = NULL;
  return klass;
}

ObjClass *newClassFromCString(const char *name) {
  ObjClass *klass;
  String *nameObj = internCString(name);
  push(valString(nameObj));
  klass = newClass(nameObj);
  pop(); /* nameObj */
  return klass;
}

ObjClass *newForeverClassFromCString(const char *name) {
  ObjClass *klass = newClassFromCString(name);
  addForeverValue(valClass(klass));
  return klass;
}

ObjClass *newClassForModule(ObjModule *module, const char *name) {
  ObjClass *cls = newClassFromCString(name);
  moduleRetain(module, valClass(cls));
  mapSetN(&module->fields, name, valClass(cls));
  return cls;
}

static ubool startsWith(const char *string, const char *prefix) {
  size_t prefixLen = strlen(prefix), stringLen = strlen(string);
  return stringLen >= prefixLen && memcmp(string, prefix, prefixLen) == 0;
}

static void addMethodsToNativeOrBuiltinClass(
    ObjClass *cls,
    NativeObjectDescriptor *descriptor,
    CFunction **methods,
    CFunction **staticMethods) {
  CFunction **function;

  if (methods) {
    for (function = methods; *function; function++) {
      if (startsWith((*function)->name, "__get_")) {
        String *name = internCString((*function)->name + strlen("__get_"));
        push(valString(name));
        mapSetStr(&cls->fieldGetters, name, valCFunction(*function));
        pop(); /* name */
      } else if (startsWith((*function)->name, "__set_")) {
        String *name = internCString((*function)->name + strlen("__set_"));
        push(valString(name));
        mapSetStr(&cls->fieldSetters, name, valCFunction(*function));
        pop(); /* name */
      } else {
        String *name = internCString((*function)->name);
        push(valString(name));
        if (name == vm.callString) {
          cls->call = *function;
        } else if (name == vm.getattrString) {
          cls->getattr = *function;
        } else if (name == vm.setattrString) {
          cls->setattr = *function;
        } else {
          mapSetStr(&cls->methods, name, valCFunction(*function));
        }
        pop(); /* name */
      }
    }
  }

  if (staticMethods) {
    for (function = staticMethods; *function; function++) {
      String *name = internCString((*function)->name);
      push(valString(name));
      mapSetStr(&cls->staticMethods, name, valCFunction(*function));
      if (name == vm.callString) {
        cls->instantiate = *function;
      }
      pop(); /* name */
    }
  }
}

ObjClass *newNativeClass(
    ObjModule *module,
    NativeObjectDescriptor *descriptor,
    CFunction **methods,
    CFunction **staticMethods) {
  ObjClass *cls = descriptor->klass = module
                                          ? newClassForModule(module, descriptor->name)
                                          : newForeverClassFromCString(descriptor->name);
  cls->descriptor = descriptor;
  addMethodsToNativeOrBuiltinClass(
      cls, descriptor, methods, staticMethods);
  return cls;
}

ObjClass *newBuiltinClass(
    const char *name,
    ObjClass **slot,
    CFunction **methods,
    CFunction **staticMethods) {
  ObjClass *cls = newForeverClassFromCString(name);
  *slot = cls;
  cls->isBuiltinClass = UTRUE;
  addMethodsToNativeOrBuiltinClass(
      cls, NULL, methods, staticMethods);
  return cls;
}

ObjClosure *newClosure(ObjThunk *thunk, ObjModule *module) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, thunk->upvalueCount);
  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  i16 i;
  for (i = 0; i < thunk->upvalueCount; i++) {
    upvalues[i] = NULL;
  }
  closure->module = module;
  closure->thunk = thunk;
  closure->upvalues = upvalues;
  closure->upvalueCount = thunk->upvalueCount;
  return closure;
}

ObjThunk *newThunk(void) {
  ObjThunk *thunk = ALLOCATE_OBJ(ObjThunk, OBJ_THUNK);
  thunk->arity = 0;
  thunk->upvalueCount = 0;
  thunk->name = NULL;
  thunk->defaultArgs = NULL;
  thunk->defaultArgsCount = 0;
  thunk->parameterNames = NULL;
  thunk->moduleName = NULL;
  initChunk(&thunk->chunk);
  return thunk;
}

ObjInstance *newInstance(ObjClass *klass) {
  ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->klass = klass;
  initMap(&instance->fields);
  instance->retainList = NULL;
  return instance;
}

static u32 hashFrozenList(Value *buffer, size_t length) {
  /* FNV-1a as presented in the Crafting Interpreters book */
  size_t i;
  u32 hash = 2166136261u;
  for (i = 0; i < length; i++) {
    u32 itemhash = hashval(buffer[i]);
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

ObjBuffer *newBuffer(void) {
  ObjBuffer *buffer = ALLOCATE_OBJ(ObjBuffer, OBJ_BUFFER);
  initBuffer(&buffer->handle);
  buffer->memoryRegionOwner = valNil();
  return buffer;
}

/* ObjBuffers created with this function should almost always
 * explicilty set the 'owner' field */
ObjBuffer *newBufferWithExternalData(Value owner, u8 *data, size_t length) {
  ObjBuffer *buffer = ALLOCATE_OBJ(ObjBuffer, OBJ_BUFFER);
  initBufferWithExternalData(&buffer->handle, data, length);
  buffer->memoryRegionOwner = owner;
  return buffer;
}

ObjList *newList(size_t size) {
  ObjList *list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
  ubool gcPause;
  list->capacity = 0;
  list->length = 0;
  list->buffer = NULL;

  LOCAL_GC_PAUSE(gcPause);

  if (size > 0) {
    size_t i;
    list->buffer = ALLOCATE(Value, size);
    list->capacity = list->length = size;
    for (i = 0; i < size; i++) {
      list->buffer[i] = valNil();
    }
  }

  LOCAL_GC_UNPAUSE(gcPause);

  return list;
}

ObjList *newListFromArray(Value *values, size_t length) {
  ObjList *list = newList(length);
  memcpy(
      (void *)(list->buffer),
      (void *)values,
      sizeof(Value) * length);
  return list;
}

ubool newListFromIterable(Value iterable, ObjList **out) {
  Value iterator, item;
  ObjList *list;
  if (isList(iterable)) {
    ObjList *other = AS_LIST_UNSAFE(iterable);
    *out = newListFromArray(other->buffer, other->length);
    return STATUS_OK;
  }
  if (isFrozenList(iterable)) {
    ObjFrozenList *other = AS_FROZEN_LIST_UNSAFE(iterable);
    *out = newListFromArray(other->buffer, other->length);
    return STATUS_OK;
  }
  if (!valueFastIter(iterable, &iterator)) {
    return STATUS_ERROR;
  }
  push(iterator);
  *out = list = newList(0);
  push(valList(list));
  while (1) {
    if (!valueFastIterNext(&iterator, &item)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(item)) {
      break;
    }
    listAppend(list, item);
  }
  pop(); /* list */
  pop(); /* iterator */
  return STATUS_OK;
}

static ObjFrozenList *allocateFrozenList(Value *buffer, int length, u32 hash) {
  ObjFrozenList *frozenList = ALLOCATE_OBJ(ObjFrozenList, OBJ_FROZEN_LIST);
  frozenList->length = length;
  frozenList->buffer = buffer;
  frozenList->hash = hash;

  push(valFrozenList(frozenList));
  mapSet(&vm.frozenLists, valFrozenList(frozenList), valNil());
  pop();

  return frozenList;
}

ObjFrozenList *copyFrozenList(Value *buffer, size_t length) {
  u32 hash = hashFrozenList(buffer, length);
  ObjFrozenList *interned = mapFindFrozenList(&vm.frozenLists, buffer, length, hash);
  Value *newBuffer;
  if (interned != NULL) {
    return interned;
  }
  newBuffer = ALLOCATE(Value, length);
  memcpy(newBuffer, buffer, sizeof(Value) * length);
  return allocateFrozenList(newBuffer, length, hash);
}

ubool newFrozenListFromIterable(Value iterable, ObjFrozenList **out) {
  ObjList *list;
  if (isFrozenList(iterable)) {
    *out = AS_FROZEN_LIST_UNSAFE(iterable);
    return STATUS_OK;
  }
  if (isList(iterable)) {
    list = AS_LIST_UNSAFE(iterable);
    *out = copyFrozenList(list->buffer, list->length);
    return STATUS_OK;
  }
  if (!newListFromIterable(iterable, &list)) {
    return STATUS_ERROR;
  }
  push(valList(list));
  *out = copyFrozenList(list->buffer, list->length);
  pop(); /* list */
  return STATUS_OK;
}

ObjDict *newDict(void) {
  ObjDict *dict = ALLOCATE_OBJ(ObjDict, OBJ_DICT);
  initMap(&dict->map);
  return dict;
}

ubool newDictFromMap(Value map, ObjDict **out) {
  ObjDict *dict = newDict();
  Value iterator, key, value;
  ubool gcPause;
  LOCAL_GC_PAUSE(gcPause);
  if (!valueFastIter(map, &iterator)) {
    return STATUS_ERROR;
  }
  for (;;) {
    if (!valueFastIterNext(&iterator, &key)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(key)) {
      break;
    }
    if (!valueGetItem(map, key, &value)) {
      return STATUS_ERROR;
    }
    mapSet(&dict->map, key, value);
  }
  LOCAL_GC_UNPAUSE(gcPause);
  *out = dict;
  return STATUS_OK;
}

ubool newDictFromPairs(Value iterable, ObjDict **out) {
  ObjDict *dict = newDict();
  Value iterator;
  push(valDict(dict));
  if (!valueFastIter(iterable, &iterator)) {
    return STATUS_ERROR;
  }
  push(iterator);
  for (;;) {
    Value pair, pairIterator, first, second, third;
    if (!valueFastIterNext(&iterator, &pair)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(pair)) {
      break;
    }
    push(pair);
    if (!valueFastIter(pair, &pairIterator)) {
      return STATUS_ERROR;
    }
    if (!valueFastIterNext(&pairIterator, &first)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(first)) {
      runtimeError("dict(): a pair is missing first item");
      return STATUS_ERROR;
    }
    push(first);
    if (!valueFastIterNext(&pairIterator, &second)) {
      return STATUS_ERROR;
    }
    if (isStopIteration(second)) {
      runtimeError("dict(): a pair is missing second item");
      return STATUS_ERROR;
    }
    push(second);
    if (!valueFastIterNext(&pairIterator, &third)) {
      return STATUS_ERROR;
    }
    if (!isStopIteration(third)) {
      runtimeError("dict(): a pair has more than two values");
      return STATUS_ERROR;
    }
    mapSet(&dict->map, first, second);
    pop(); /* second */
    pop(); /* first */
    pop(); /* pair */
  }
  pop(); /* iterator */
  pop(); /* dict */
  *out = dict;
  return STATUS_OK;
}

static ObjFrozenDict *newFrozenDictWithHash(Map *map, u32 hash) {
  ObjFrozenDict *fdict = mapFindFrozenDict(&vm.frozenDicts, map, hash);
  if (fdict != NULL) {
    return fdict;
  }
  fdict = ALLOCATE_OBJ(ObjFrozenDict, OBJ_FROZEN_DICT);
  fdict->hash = hash;
  initMap(&fdict->map);
  push(valFrozenDict(fdict));
  mapAddAll(map, &fdict->map);
  mapSet(&vm.frozenDicts, valFrozenDict(fdict), valNil());
  pop();
  return fdict;
}

static u32 hashMap(Map *map) {
  /* Essentially the CPython frozenset hash algorithm described here:
   * https://stackoverflow.com/questions/20832279/ */
  u32 hash = 1927868237UL;
  MapIterator mi;
  MapEntry *entry;
  hash *= 2 * map->size * 2;
  initMapIterator(&mi, map);
  while (mapIteratorNext(&mi, &entry)) {
    u32 kh = hashval(entry->key);
    u32 vh = hashval(entry->value);
    hash ^= (kh ^ (kh << 16) ^ 89869747UL) * 3644798167UL;
    hash ^= (vh ^ (vh << 16) ^ 89869747UL) * 3644798167UL;
  }
  hash = hash * 69069U + 907133923UL;
  return hash;
}

ObjFrozenDict *newFrozenDict(Map *map) {
  return newFrozenDictWithHash(map, hashMap(map));
}

ObjNative *newNative(NativeObjectDescriptor *descriptor, size_t objectSize) {
  ObjNative *n;
  if (descriptor->objectSize != objectSize) {
    panic("Mismatched native object size");
  }
  n = (ObjNative *)allocateObject(objectSize, OBJ_NATIVE, descriptor->name);
  n->descriptor = descriptor;
  return n;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = valNil();
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

ObjClass *getClassOfValue(Value value) {
  switch (value.type) {
    case VAL_NIL:
      return vm.nilClass;
    case VAL_BOOL:
      return vm.boolClass;
    case VAL_NUMBER:
      return vm.numberClass;
    case VAL_STRING:
      return vm.stringClass;
    case VAL_CFUNCTION:
      return vm.functionClass;
    case VAL_SENTINEL:
      return vm.sentinelClass;
    case VAL_RANGE:
      return vm.rangeClass;
    case VAL_RANGE_ITERATOR:
      return vm.rangeIteratorClass;
    case VAL_VECTOR:
      return vm.vectorClass;
    case VAL_POINTER:
      return vm.pointerClass;
    case VAL_OBJ: {
      switch (AS_OBJ_UNSAFE(value)->type) {
        case OBJ_CLASS:
          return vm.classClass;
        case OBJ_CLOSURE:
          return vm.functionClass;
        case OBJ_THUNK:
          panic("thunk kinds do not have classes");
        case OBJ_INSTANCE:
          return AS_INSTANCE_UNSAFE(value)->klass;
        case OBJ_BUFFER:
          return vm.bufferClass;
        case OBJ_LIST:
          return vm.listClass;
        case OBJ_FROZEN_LIST:
          return vm.frozenListClass;
        case OBJ_DICT:
          return vm.dictClass;
        case OBJ_FROZEN_DICT:
          return vm.frozenDictClass;
        case OBJ_NATIVE:
          return AS_NATIVE_UNSAFE(value)->descriptor->klass;
        case OBJ_UPVALUE:
          panic("upvalue kinds do not have classes");
      }
      break;
    }
  }
  panic("Class not found for %s kinds", getKindName(value));
  return NULL;
}

ubool classHasMethod(ObjClass *cls, String *name) {
  Value method;
  return cls && mapGet(&cls->methods, valString(name), &method);
}

static void printFunction(ObjThunk *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_CLASS:
      if (AS_CLASS_UNSAFE(value)->isModuleClass) {
        printf("<module %s>", AS_CLASS_UNSAFE(value)->name->chars);
      } else {
        printf("<class %s>", AS_CLASS_UNSAFE(value)->name->chars);
      }
      return;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE_UNSAFE(value)->thunk);
      return;
    case OBJ_THUNK:
      printFunction(AS_THUNK_UNSAFE(value));
      return;
    case OBJ_INSTANCE:
      printf("<%s instance>", AS_INSTANCE_UNSAFE(value)->klass->name->chars);
      return;
    case OBJ_BUFFER:
      printf("<buffer %lu>", (unsigned long)AS_BUFFER_UNSAFE(value)->handle.length);
      return;
    case OBJ_LIST:
      printf("<list %lu items>", (unsigned long)AS_LIST_UNSAFE(value)->length);
      return;
    case OBJ_FROZEN_LIST:
      printf("<frozenList %lu items>", (unsigned long)AS_FROZEN_LIST_UNSAFE(value)->length);
      return;
    case OBJ_DICT:
      printf("<dict>");
      return;
    case OBJ_FROZEN_DICT:
      printf("<frozendict>");
      return;
    case OBJ_NATIVE:
      printf(
          "<native-object %s>",
          AS_NATIVE_UNSAFE(value)->descriptor->klass->name->chars);
      return;
    case OBJ_UPVALUE:
      printf("<upvalue>");
      return;
  }
  abort();
}

const char *getObjectTypeName(ObjType type) {
  switch (type) {
    case OBJ_CLASS:
      return "OBJ_CLASS";
    case OBJ_CLOSURE:
      return "OBJ_CLOSURE";
    case OBJ_THUNK:
      return "OBJ_THUNK";
    case OBJ_INSTANCE:
      return "OBJ_INSTANCE";
    case OBJ_BUFFER:
      return "OBJ_BUFFER";
    case OBJ_LIST:
      return "OBJ_LIST";
    case OBJ_FROZEN_LIST:
      return "OBJ_FROZEN_LIST";
    case OBJ_DICT:
      return "OBJ_DICT";
    case OBJ_FROZEN_DICT:
      return "OBJ_FROZEN_DICT";
    case OBJ_NATIVE:
      return "OBJ_NATIVE";
    case OBJ_UPVALUE:
      return "OBJ_UPVALUE";
  }
  return "OBJ_<unrecognized>";
}

ubool isNative(Value value, NativeObjectDescriptor *descriptor) {
  return isNativeObj(value) && AS_NATIVE_UNSAFE(value)->descriptor == descriptor;
}

Value valList(ObjList *list) {
  return valObjExplicit((Obj *)list);
}

Value valDict(ObjDict *dict) {
  return valObjExplicit((Obj *)dict);
}

Value valFrozenDict(ObjFrozenDict *fdict) {
  return valObjExplicit((Obj *)fdict);
}

Value valInstance(ObjInstance *instance) {
  return valObjExplicit((Obj *)instance);
}

Value valModule(ObjModule *module) {
  return valObjExplicit((Obj *)module);
}

Value valBuffer(ObjBuffer *buffer) {
  return valObjExplicit((Obj *)buffer);
}

Value valThunk(ObjThunk *thunk) {
  return valObjExplicit((Obj *)thunk);
}

Value valClosure(ObjClosure *closure) {
  return valObjExplicit((Obj *)closure);
}

Value valFrozenList(ObjFrozenList *frozenList) {
  return valObjExplicit((Obj *)frozenList);
}

Value valClass(ObjClass *klass) {
  return valObjExplicit((Obj *)klass);
}

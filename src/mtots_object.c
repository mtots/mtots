#include "mtots_vm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType, NULL)

/* should-be-inline */ ubool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

/* Returns the value's native object descriptor if the value is a
 * native value. Otherwise returns NULL. */
NativeObjectDescriptor *getNativeObjectDescriptor(Value value) {
  if (IS_NATIVE(value)) {
    return AS_NATIVE(value)->descriptor;
  }
  return NULL;
}

void nopBlacken(ObjNative *n) {}
void nopFree(ObjNative *n) {}

static Obj *allocateObject(size_t size, ObjType type, const char *typeName) {
  Obj *object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = UFALSE;
  object->next = vm.memory.objects;
  vm.memory.objects = object;
  if (vm.enableMallocFreeLogs) {
    eprintln(
      "DEBUG: allocate Object %s at %p",
      typeName ? typeName : getObjectTypeName(type),
      (void*)object);
  }
  return object;
}

ubool IS_MODULE(Value value) {
  return IS_INSTANCE(value) && AS_INSTANCE(value)->klass->isModuleClass;
}

ObjModule *newModule(String *name, ubool includeGlobals) {
  ObjClass *klass;
  ObjModule *instance;

  klass = newClass(name);
  klass->isModuleClass = UTRUE;

  push(CLASS_VAL(klass));
  instance = newInstance(klass);
  pop(); /* klass */

  if (includeGlobals) {
    push(INSTANCE_VAL(instance));
    mapAddAll(&vm.globals, &instance->fields);
    pop(); /* instance */
  }

  return instance;
}

ObjModule *newModuleFromCString(const char *name, ubool includeGlobals) {
  String *nameStr;
  ObjModule *instance;

  nameStr = internCString(name);
  push(STRING_VAL(nameStr));
  instance = newModule(nameStr, includeGlobals);
  pop(); /* nameStr */

  return instance;
}

void moduleAddFunctions(ObjModule *module, CFunction **functions) {
  CFunction **function;
  for (function = functions; *function; function++) {
    mapSetN(&module->fields, (*function)->name, CFUNCTION_VAL(*function));
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
  moduleRetain(module, STRING_VAL(string));
  return string;
}

ObjClass *newClass(String *name) {
  ObjClass *klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name;
  initMap(&klass->methods);
  initMap(&klass->staticMethods);
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
  push(STRING_VAL(nameObj));
  klass = newClass(nameObj);
  pop(); /* nameObj */
  return klass;
}

ObjClass *newForeverClassFromCString(const char *name) {
  ObjClass *klass = newClassFromCString(name);
  addForeverValue(CLASS_VAL(klass));
  return klass;
}

ObjClass *newClassForModule(ObjModule *module, const char *name) {
  ObjClass *cls = newClassFromCString(name);
  moduleRetain(module, CLASS_VAL(cls));
  mapSetN(&module->fields, name, CLASS_VAL(cls));
  return cls;
}

static void addMethodsToNativeOrBuiltinClass(
    ObjClass *cls,
    TypePatternType typePatternType,
    NativeObjectDescriptor *descriptor,
    CFunction **methods,
    CFunction **staticMethods) {
  CFunction **function;

  if (methods) {
    for (function = methods; *function; function++) {
      String *name = internCString((*function)->name);
      push(STRING_VAL(name));
      (*function)->receiverType.type = typePatternType;
      (*function)->receiverType.nativeTypeDescriptor = descriptor;
      if (name == vm.callString) {
        cls->call = *function;
      } else if (name == vm.getattrString) {
        cls->getattr = *function;
      } else if (name == vm.setattrString) {
        cls->setattr = *function;
      } else {
        mapSetStr(&cls->methods, name, CFUNCTION_VAL(*function));
      }
      pop(); /* name */
    }
  }

  if (staticMethods) {
    for (function = staticMethods; *function; function++) {
      String *name = internCString((*function)->name);
      push(STRING_VAL(name));
      mapSetStr(&cls->staticMethods, name, CFUNCTION_VAL(*function));
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
  ObjClass *cls = descriptor->klass = module ?
    newClassForModule(module, descriptor->name) :
    newForeverClassFromCString(descriptor->name);
  cls->descriptor = descriptor;
  addMethodsToNativeOrBuiltinClass(
    cls, TYPE_PATTERN_NATIVE, descriptor, methods, staticMethods);
  return cls;
}

ObjClass *newBuiltinClass(
    const char *name,
    ObjClass **slot,
    TypePatternType typePatternType,
    CFunction **methods,
    CFunction **staticMethods) {
  ObjClass *cls = newForeverClassFromCString(name);
  *slot = cls;
  cls->isBuiltinClass = UTRUE;
  addMethodsToNativeOrBuiltinClass(
    cls, typePatternType, NULL, methods, staticMethods);
  return cls;
}

ObjClosure *newClosure(ObjThunk *thunk, ObjModule *module) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue*, thunk->upvalueCount);
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
    hash ^= (u8) (itemhash);
    hash *= 16777619;
    hash ^= (u8) (itemhash >> 8);
    hash *= 16777619;
    hash ^= (u8) (itemhash >> 16);
    hash *= 16777619;
    hash ^= (u8) (itemhash >> 24);
    hash *= 16777619;
  }
  return hash;
}

ObjBuffer *newBuffer(void) {
  ObjBuffer *buffer = ALLOCATE_OBJ(ObjBuffer, OBJ_BUFFER);
  initBuffer(&buffer->handle);
  buffer->memoryRegionOwner = NIL_VAL();
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
      list->buffer[i] = NIL_VAL();
    }
  }

  LOCAL_GC_UNPAUSE(gcPause);

  return list;
}

ObjList *newListFromArray(Value *values, size_t length) {
  ObjList *list = newList(length);
  memcpy(
    (void*)(list->buffer),
    (void*)values,
    sizeof(Value) * length);
  return list;
}

ubool newListFromIterable(Value iterable, ObjList **out) {
  Value iterator, item;
  ObjList *list;
  if (IS_LIST(iterable)) {
    ObjList *other = AS_LIST(iterable);
    *out = newListFromArray(other->buffer, other->length);
    return UTRUE;
  }
  if (IS_FROZEN_LIST(iterable)) {
    ObjFrozenList *other = AS_FROZEN_LIST(iterable);
    *out = newListFromArray(other->buffer, other->length);
    return UTRUE;
  }
  if (!valueFastIter(iterable, &iterator)) {
    return UFALSE;
  }
  push(iterator);
  *out = list = newList(0);
  push(LIST_VAL(list));
  while (1) {
    if (!valueFastIterNext(&iterator, &item)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(item)) {
      break;
    }
    listAppend(list, item);
  }
  pop(); /* list */
  pop(); /* iterator */
  return UTRUE;
}

static ObjFrozenList *allocateFrozenList(Value *buffer, int length, u32 hash) {
  ObjFrozenList *frozenList = ALLOCATE_OBJ(ObjFrozenList, OBJ_FROZEN_LIST);
  frozenList->length = length;
  frozenList->buffer = buffer;
  frozenList->hash = hash;

  push(FROZEN_LIST_VAL(frozenList));
  mapSet(&vm.frozenLists, FROZEN_LIST_VAL(frozenList), NIL_VAL());
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
  if (IS_FROZEN_LIST(iterable)) {
    *out = AS_FROZEN_LIST(iterable);
    return UTRUE;
  }
  if (IS_LIST(iterable)) {
    list = AS_LIST(iterable);
    *out = copyFrozenList(list->buffer, list->length);
    return UTRUE;
  }
  if (!newListFromIterable(iterable, &list)) {
    return UFALSE;
  }
  push(LIST_VAL(list));
  *out = copyFrozenList(list->buffer, list->length);
  pop(); /* list */
  return UTRUE;
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
    return UFALSE;
  }
  for (;;) {
    if (!valueFastIterNext(&iterator, &key)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(key)) {
      break;
    }
    if (!valueGetItem(map, key, &value)) {
      return UFALSE;
    }
    mapSet(&dict->map, key, value);
  }
  LOCAL_GC_UNPAUSE(gcPause);
  *out = dict;
  return UTRUE;
}

ubool newDictFromPairs(Value iterable, ObjDict **out) {
  ObjDict *dict = newDict();
  Value iterator;
  push(DICT_VAL(dict));
  if (!valueFastIter(iterable, &iterator)) {
    return UFALSE;
  }
  push(iterator);
  for (;;) {
    Value pair, pairIterator, first, second, third;
    if (!valueFastIterNext(&iterator, &pair)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(pair)) {
      break;
    }
    push(pair);
    if (!valueFastIter(pair, &pairIterator)) {
      return UFALSE;
    }
    if (!valueFastIterNext(&pairIterator, &first)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(first)) {
      runtimeError("dict(): a pair is missing first item");
      return UFALSE;
    }
    push(first);
    if (!valueFastIterNext(&pairIterator, &second)) {
      return UFALSE;
    }
    if (IS_STOP_ITERATION(second)) {
      runtimeError("dict(): a pair is missing second item");
      return UFALSE;
    }
    push(second);
    if (!valueFastIterNext(&pairIterator, &third)) {
      return UFALSE;
    }
    if (!IS_STOP_ITERATION(third)) {
      runtimeError("dict(): a pair has more than two values");
      return UFALSE;
    }
    mapSet(&dict->map, first, second);
    pop(); /* second */
    pop(); /* first */
    pop(); /* pair */
  }
  pop(); /* iterator */
  pop(); /* dict */
  *out = dict;
  return UTRUE;
}

static ObjFrozenDict *newFrozenDictWithHash(Map *map, u32 hash) {
  ObjFrozenDict *fdict = mapFindFrozenDict(&vm.frozenDicts, map, hash);
  if (fdict != NULL) {
    return fdict;
  }
  fdict = ALLOCATE_OBJ(ObjFrozenDict, OBJ_FROZEN_DICT);
  fdict->hash = hash;
  initMap(&fdict->map);
  push(FROZEN_DICT_VAL(fdict));
  mapAddAll(map, &fdict->map);
  mapSet(&vm.frozenDicts, FROZEN_DICT_VAL(fdict), NIL_VAL());
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
    hash ^= (kh ^ (kh << 16) ^ 89869747UL)  * 3644798167UL;
    hash ^= (vh ^ (vh << 16) ^ 89869747UL)  * 3644798167UL;
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
  n = (ObjNative*)allocateObject(objectSize, OBJ_NATIVE, descriptor->name);
  n->descriptor = descriptor;
  return n;
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL();
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

ObjClass *getClassOfValue(Value value) {
  switch (value.type) {
    case VAL_NIL: return vm.nilClass;
    case VAL_BOOL: return vm.boolClass;
    case VAL_NUMBER: return vm.numberClass;
    case VAL_STRING: return vm.stringClass;
    case VAL_CFUNCTION: return vm.functionClass;
    case VAL_SENTINEL: return vm.sentinelClass;
    case VAL_FAST_RANGE: return vm.fastRangeClass;
    case VAL_FAST_RANGE_ITERATOR: return vm.fastRangeIteratorClass;
    case VAL_FAST_LIST_ITERATOR: return vm.fastListIteratorClass;
    case VAL_COLOR: return vm.colorClass;
    case VAL_VECTOR: return vm.vectorClass;
    case VAL_RECT: return vm.rectClass;
    case VAL_OBJ: {
      switch (AS_OBJ(value)->type) {
        case OBJ_CLASS: return vm.classClass;
        case OBJ_CLOSURE: return vm.functionClass;
        case OBJ_THUNK: panic("thunk kinds do not have classes");
        case OBJ_INSTANCE: return AS_INSTANCE(value)->klass;
        case OBJ_BUFFER: return vm.bufferClass;
        case OBJ_LIST: return vm.listClass;
        case OBJ_FROZEN_LIST: return vm.frozenListClass;
        case OBJ_DICT: return vm.dictClass;
        case OBJ_FROZEN_DICT: return vm.frozenDictClass;
        case OBJ_NATIVE: return AS_NATIVE(value)->descriptor->klass;
        case OBJ_UPVALUE: panic("upvalue kinds do not have classes");
      }
      break;
    }
  }
  panic("Class not found for %s kinds", getKindName(value));
  return NULL;
}

ubool classHasMethod(ObjClass *cls, String *name) {
  Value method;
  return cls && mapGet(&cls->methods, STRING_VAL(name), &method);
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
      if (AS_CLASS(value)->isModuleClass) {
        printf("<module %s>", AS_CLASS(value)->name->chars);
      } else {
        printf("<class %s>", AS_CLASS(value)->name->chars);
      }
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->thunk);
      break;
    case OBJ_THUNK:
      printFunction(AS_THUNK(value));
      break;
    case OBJ_INSTANCE:
      printf("<%s instance>", AS_INSTANCE(value)->klass->name->chars);
      break;
    case OBJ_BUFFER:
      printf("<buffer %lu>", (unsigned long)AS_BUFFER(value)->handle.length);
      break;
    case OBJ_LIST:
      printf("<list %lu items>", (unsigned long) AS_LIST(value)->length);
      break;
    case OBJ_FROZEN_LIST:
      printf("<frozenList %lu items>", (unsigned long) AS_FROZEN_LIST(value)->length);
      break;
    case OBJ_DICT:
      printf("<dict>");
      break;
    case OBJ_FROZEN_DICT:
      printf("<frozendict>");
      break;
    case OBJ_NATIVE:
      printf(
        "<native-object %s>",
        AS_NATIVE(value)->descriptor->klass->name->chars);
      break;
    case OBJ_UPVALUE:
      printf("<upvalue>");
      break;
    default:
      abort();
  }
}

const char *getObjectTypeName(ObjType type) {
  switch (type) {
  case OBJ_CLASS: return "OBJ_CLASS";
  case OBJ_CLOSURE: return "OBJ_CLOSURE";
  case OBJ_THUNK: return "OBJ_THUNK";
  case OBJ_INSTANCE: return "OBJ_INSTANCE";
  case OBJ_BUFFER: return "OBJ_BUFFER";
  case OBJ_LIST: return "OBJ_LIST";
  case OBJ_FROZEN_LIST: return "OBJ_FROZEN_LIST";
  case OBJ_DICT: return "OBJ_DICT";
  case OBJ_FROZEN_DICT: return "OBJ_FROZEN_DICT";
  case OBJ_NATIVE: return "OBJ_NATIVE";
  case OBJ_UPVALUE: return "OBJ_UPVALUE";
  }
  return "OBJ_<unrecognized>";
}

ubool isNative(Value value, NativeObjectDescriptor *descriptor) {
  return IS_NATIVE(value) && AS_NATIVE(value)->descriptor == descriptor;
}

Value LIST_VAL(ObjList *list) {
  return OBJ_VAL_EXPLICIT((Obj*)list);
}

Value DICT_VAL(ObjDict *dict) {
  return OBJ_VAL_EXPLICIT((Obj*)dict);
}

Value FROZEN_DICT_VAL(ObjFrozenDict *fdict) {
  return OBJ_VAL_EXPLICIT((Obj*)fdict);
}

Value INSTANCE_VAL(ObjInstance *instance) {
  return OBJ_VAL_EXPLICIT((Obj*)instance);
}

Value MODULE_VAL(ObjModule *module) {
  return OBJ_VAL_EXPLICIT((Obj*)module);
}

Value BUFFER_VAL(ObjBuffer *buffer) {
  return OBJ_VAL_EXPLICIT((Obj*)buffer);
}

Value THUNK_VAL(ObjThunk *thunk) {
  return OBJ_VAL_EXPLICIT((Obj*)thunk);
}

Value CLOSURE_VAL(ObjClosure *closure) {
  return OBJ_VAL_EXPLICIT((Obj*)closure);
}

Value FROZEN_LIST_VAL(ObjFrozenList *frozenList) {
  return OBJ_VAL_EXPLICIT((Obj*)frozenList);
}

Value CLASS_VAL(ObjClass *klass) {
  return OBJ_VAL_EXPLICIT((Obj*)klass);
}

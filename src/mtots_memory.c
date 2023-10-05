#include <stdlib.h>
#include <string.h>

#include "mtots_parser.h"
#include "mtots_vm.h"

#define GC_HEAP_GROW_FACTOR 2

void initMemory(Memory *memory) {
  memory->bytesAllocated = 0;
  memory->nextGC = 1024 * 1024;
  memory->objects = NULL;
  memory->grayCount = 0;
  memory->grayCapacity = 0;
  memory->grayStack = NULL;
  memory->foreverValueCount = 0;
  memory->mallocCount = 0;
}

void addForeverValue(Value value) {
  if (vm.memory.foreverValueCount >= MAX_FOREVER_VALUE_COUNT) {
    panic("Too many forever objects (max=%d)", MAX_FOREVER_VALUE_COUNT);
  }
  vm.memory.foreverValues[vm.memory.foreverValueCount++] = value;
}

String *internForeverCString(const char *cstr) {
  String *str = internString(cstr, strlen(cstr));
  addForeverValue(valString(str));
  return str;
}

String *internForeverString(const char *chars, size_t len) {
  String *str = internString(chars, len);
  addForeverValue(valString(str));
  return str;
}

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  void *result;

  vm.memory.bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
#if DEBUG_STRESS_GC
    collectGarbage();
#endif
    vm.memory.mallocCount++;
    if (vm.memory.bytesAllocated + getInternedStringsAllocationSize() > vm.memory.nextGC) {
      collectGarbage();
    }
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  result = realloc(pointer, newSize);
  if (result == NULL) {
    panic("out of memory");
  }
  return result;
}

void markObject(Obj *object) {
  if (object == NULL || object->isMarked) {
    return;
  }
  object->isMarked = UTRUE;

  if (vm.memory.grayCapacity < vm.memory.grayCount + 1) {
    vm.memory.grayCapacity = GROW_CAPACITY(vm.memory.grayCapacity);
    vm.memory.grayStack = (Obj **)realloc(
        vm.memory.grayStack, sizeof(Obj *) * vm.memory.grayCapacity);
    if (vm.memory.grayStack == NULL) {
      panic("out of memory (during gc)");
    }
  }
  vm.memory.grayStack[vm.memory.grayCount++] = object;
}

void markString(String *string) {
  if (string) {
    string->isMarked = UTRUE;
  }
}

void markValue(Value value) {
  switch (value.type) {
    case VAL_STRING:
      markString(value.as.string);
      break;
    case VAL_OBJ:
      markObject(value.as.obj);
      break;
    default:
      break;
  }
}

static void markArray(ValueArray *array) {
  size_t i;
  for (i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj *object) {
  switch (object->type) {
    case OBJ_CLASS: {
      ObjClass *klass = (ObjClass *)object;
      markString(klass->name);
      markMap(&klass->methods);
      markMap(&klass->staticMethods);
      return;
    }
    case OBJ_CLOSURE: {
      i16 i;
      ObjClosure *closure = (ObjClosure *)object;
      markObject((Obj *)closure->module);
      markObject((Obj *)closure->thunk);
      for (i = 0; i < closure->upvalueCount; i++) {
        markObject((Obj *)closure->upvalues[i]);
      }
      return;
    }
    case OBJ_THUNK: {
      ObjThunk *thunk = (ObjThunk *)object;
      size_t i;
      markString(thunk->name);
      markString(thunk->moduleName);
      markArray(&thunk->chunk.constants);
      for (i = 0; i < thunk->defaultArgsCount; i++) {
        markValue(thunk->defaultArgs[i]);
      }
      return;
    }
    case OBJ_INSTANCE: {
      ObjInstance *instance = (ObjInstance *)object;
      markObject((Obj *)instance->klass);
      markMap(&instance->fields);
      markObject((Obj *)instance->retainList);
      return;
    }
    case OBJ_UPVALUE:
      markValue(((ObjUpvalue *)object)->closed);
      return;
    case OBJ_BUFFER:
      markValue(((ObjBuffer *)object)->memoryRegionOwner);
      return;
    case OBJ_LIST: {
      ObjList *list = (ObjList *)object;
      size_t i;
      for (i = 0; i < list->length; i++) {
        markValue(list->buffer[i]);
      }
      return;
    }
    case OBJ_FROZEN_LIST: {
      ObjFrozenList *frozenList = (ObjFrozenList *)object;
      size_t i;
      for (i = 0; i < frozenList->length; i++) {
        markValue(frozenList->buffer[i]);
      }
      return;
    }
    case OBJ_DICT: {
      ObjDict *dict = (ObjDict *)object;
      markMap(&dict->map);
      return;
    }
    case OBJ_FROZEN_DICT: {
      ObjFrozenDict *dict = (ObjFrozenDict *)object;
      markMap(&dict->map);
      return;
    }
    case OBJ_NATIVE: {
      ObjNative *n = (ObjNative *)object;
      n->descriptor->blacken(n);
      return;
    }
  }
  abort();
}

static void freeObject(Obj *object) {
  if (vm.enableMallocFreeLogs) {
    eprintln(
        "DEBUG: free     Object %s at %p",
        object->type == OBJ_NATIVE ? ((ObjNative *)object)->descriptor->name : getObjectTypeName(object->type),
        (void *)object);
  }
  switch (object->type) {
    case OBJ_CLASS: {
      ObjClass *klass = (ObjClass *)object;
      freeMap(&klass->methods);
      freeMap(&klass->staticMethods);
      FREE(ObjClass, object);
      return;
    }
    case OBJ_CLOSURE: {
      ObjClosure *closure = (ObjClosure *)object;
      FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount);
      FREE(ObjClosure, object);
      return;
    }
    case OBJ_THUNK: {
      ObjThunk *thunk = (ObjThunk *)object;
      freeChunk(&thunk->chunk);
      free(thunk->parameterNames);
      FREE_ARRAY(Value, thunk->defaultArgs, thunk->defaultArgsCount);
      FREE(ObjThunk, object);
      return;
    }
    case OBJ_INSTANCE: {
      ObjInstance *instance = (ObjInstance *)object;
      freeMap(&instance->fields);
      FREE(ObjInstance, object);
      return;
    }
    case OBJ_BUFFER: {
      ObjBuffer *buffer = (ObjBuffer *)object;
      freeBuffer(&buffer->handle);
      FREE(ObjBuffer, object);
      return;
    }
    case OBJ_LIST: {
      ObjList *list = (ObjList *)object;
      FREE_ARRAY(Value, list->buffer, list->capacity);
      FREE(ObjList, object);
      return;
    }
    case OBJ_FROZEN_LIST: {
      ObjFrozenList *frozenList = (ObjFrozenList *)object;
      FREE_ARRAY(Value, frozenList->buffer, frozenList->length);
      FREE(ObjFrozenList, object);
      return;
    }
    case OBJ_DICT: {
      ObjDict *dict = (ObjDict *)object;
      freeMap(&dict->map);
      FREE(ObjDict, object);
      return;
    }
    case OBJ_FROZEN_DICT: {
      ObjFrozenDict *dict = (ObjFrozenDict *)object;
      freeMap(&dict->map);
      FREE(ObjFrozenDict, object);
      return;
    }
    case OBJ_NATIVE: {
      ObjNative *n = (ObjNative *)object;
      n->descriptor->free(n);
      reallocate(object, n->descriptor->objectSize, 0);
      return;
    }
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      return;
  }
  abort();
}

static void markRoots(void) {
  Value *slot;
  i16 i;
  ObjUpvalue *upvalue;
  for (slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  for (i = 0; i < SIGNAL_HANDLERS_COUNT; i++) {
    markValue(vm.signalHandlers[i]);
  }

  for (i = 0; i < vm.frameCount; i++) {
    markObject((Obj *)vm.frames[i].closure);
  }

  for (upvalue = vm.openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj *)upvalue);
  }

  for (i = 0; i < vm.memory.foreverValueCount; i++) {
    markValue(vm.memory.foreverValues[i]);
  }

  markMap(&vm.globals);
  markMap(&vm.modules);
  markMap(&vm.nativeModuleThunks);
  markParserRoots();
}

static void traceReferences(void) {
  while (vm.memory.grayCount > 0) {
    Obj *object = vm.memory.grayStack[--vm.memory.grayCount];
    blackenObject(object);
  }
}

static void sweep(void) {
  Obj *previous = NULL;
  Obj *object = vm.memory.objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = UFALSE;
      previous = object;
      object = object->next;
    } else {
      Obj *unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.memory.objects = object;
      }
      freeObject(unreached);
    }
  }
}

void freeObjects(void) {
  Obj *object = vm.memory.objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }

  free(vm.memory.grayStack);
}

static size_t countObjects(void) {
  size_t count = 0;
  Obj *object;
  for (object = vm.memory.objects; object; object = object->next) {
    count++;
  }
  return count;
}

void collectGarbage(void) {
  size_t before, objectCountBefore;
  const ubool emitLog = vm.enableGCLogs || vm.enableLogOnGC;

  if (vm.localGCPause) {
    return;
  }

  if (emitLog) {
    before = vm.memory.bytesAllocated + getInternedStringsAllocationSize();
    objectCountBefore = countObjects();
    eprintln("DEBUG: Starting garbage collector");
  }

  markRoots();
  traceReferences();
  freeUnmarkedStrings();
  mapRemoveWhite(&vm.frozenLists);
  mapRemoveWhite(&vm.frozenDicts);
  sweep();

  vm.memory.nextGC =
      (vm.memory.bytesAllocated + getInternedStringsAllocationSize()) *
      GC_HEAP_GROW_FACTOR;

  if (emitLog) {
    eprintln(
        "DEBUG: Finished collecting garbage\n"
        "       collected %lu bytes (from %lu to %lu) next at %lu\n"
        "       object-count = %lu -> %lu",
        (unsigned long)before - (vm.memory.bytesAllocated +
                                 getInternedStringsAllocationSize()),
        (unsigned long)before,
        (unsigned long)vm.memory.bytesAllocated +
            getInternedStringsAllocationSize(),
        (unsigned long)vm.memory.nextGC,
        (unsigned long)objectCountBefore,
        (unsigned long)countObjects());
  }
}

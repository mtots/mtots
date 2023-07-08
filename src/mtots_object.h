#ifndef mtots_object_h
#define mtots_object_h

#include "mtots_common.h"
#include "mtots_chunk.h"
#include "mtots_map.h"
#include "mtots_memory.h"

#include <stdio.h>

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_THUNK(value) isObjType(value, OBJ_THUNK)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_BUFFER(value) isObjType(value, OBJ_BUFFER)
#define IS_LIST(value) isObjType(value, OBJ_LIST)
#define IS_FROZEN_LIST(value) isObjType(value, OBJ_FROZEN_LIST)
#define IS_DICT(value) isObjType(value, OBJ_DICT)
#define IS_FROZEN_DICT(value) isObjType(value, OBJ_FROZEN_DICT)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)

#define AS_CLASS(value) ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*)AS_OBJ(value))
#define AS_THUNK(value) ((ObjThunk*)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance*)AS_OBJ(value))
#define AS_MODULE(value) ((ObjModule*)AS_OBJ(value))
#define AS_BUFFER(value) ((ObjBuffer*)AS_OBJ(value))
#define AS_LIST(value) ((ObjList*)AS_OBJ(value))
#define AS_FROZEN_LIST(value) ((ObjFrozenList*)AS_OBJ(value))
#define AS_DICT(value) ((ObjDict*)AS_OBJ(value))
#define AS_FROZEN_DICT(value) ((ObjFrozenDict*)AS_OBJ(value))
#define AS_NATIVE(value) ((ObjNative*)AS_OBJ(value))

#define NEW_NATIVE(type, descriptor) \
  ((type*)newNative(descriptor, sizeof(type)))

typedef struct ObjNative ObjNative;
typedef struct ObjClass ObjClass;
typedef struct ObjInstance ObjInstance;
typedef struct ObjInstance ObjModule;   /* Modules alias to instances for now */

typedef enum ObjType {
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_THUNK,
  OBJ_INSTANCE,
  OBJ_BUFFER,
  OBJ_LIST,
  OBJ_FROZEN_LIST,
  OBJ_DICT,
  OBJ_FROZEN_DICT,

  OBJ_NATIVE,

  OBJ_UPVALUE
} ObjType;

struct Obj {
  ObjType type;
  ubool isMarked;
  struct Obj *next;
};

typedef struct ObjThunk {
  Obj obj;
  i16 arity;
  i16 upvalueCount;
  Chunk chunk;
  String *name;
  Value *defaultArgs;
  i16 defaultArgsCount;
  String *moduleName;
} ObjThunk;

/**
 * Buffer object for manipulating raw bytes.
 * NOTE: the location of the raw bytes is not stable because
 * the buffer may reallocate memory when appending bytes.
 */
typedef struct ObjBuffer {
  Obj obj;
  Buffer handle;

  /* The owner of the memory region pointed to by this ObjBuffer.
   * (Not the owner of this ObjBuffer).
   *
   * If the memory region pointed to by this ObjBuffer/Buffer is
   * owned by another Value, hold on a reference to the owner here.
   * If the memory region is owned by this ObjBuffer,
   * `memoryRegionOwner` should be nil */
  Value memoryRegionOwner;
} ObjBuffer;

typedef struct ObjList {
  Obj obj;
  size_t length;
  size_t capacity;
  Value *buffer;
} ObjList;

/* Unlike in Python, mtots frozenLists can only hold hashable items */
typedef struct ObjFrozenList {
  Obj obj;
  size_t length;
  Value *buffer;
  u32 hash;
} ObjFrozenList;

typedef struct ObjDict {
  Obj obj;
  Map map;
} ObjDict;

typedef struct ObjFrozenDict {
  Obj obj;
  Map map;
  u32 hash;
} ObjFrozenDict;

typedef struct NativeObjectDescriptor {
  void (*blacken)(ObjNative*);
  void (*free)(ObjNative*);

  size_t objectSize;
  const char *name;

  /* Should be initialized as soon as the relevant native module
   * is loaded */
  ObjClass *klass;
} NativeObjectDescriptor;

struct ObjNative {
  Obj obj;
  NativeObjectDescriptor *descriptor;
};

typedef struct ObjUpvalue {
  Obj obj;
  Value *location;
  Value closed;
  struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct ObjClosure {
  Obj obj;
  ObjModule *module;
  ObjThunk *thunk;
  ObjUpvalue **upvalues;
  i16 upvalueCount;
} ObjClosure;

struct ObjClass {
  Obj obj;
  String *name;
  Map methods;
  Map staticMethods;
  ubool isModuleClass;
  ubool isBuiltinClass;
  NativeObjectDescriptor *descriptor; /* NULL if not native */

  /*
   * If set, will override the behavior of instantiating the class.
   *
   * Useful for builtin and native classes.
   */
  CFunction *instantiate;

  /*
   * If set, allows values that are not normally callable to be callable.
   *
   * For functions and classes, this field is ignored.
   *
   * Useful for builtin and native classes.
   */
  CFunction *call;

  /* If set, allows attribute access for native object instances. */
  CFunction *getattr;

  /* If set, allows attribute assignment for native object instances. */
  CFunction *setattr;
};

struct ObjInstance {
  Obj obj;
  ObjClass *klass;
  Map fields;

  /**
   * Some hidden objects this instance may hold references to.
   * Primary use is for native modules that may want to retain
   * references to values not directly referenced in its fields.
   */
  ObjList *retainList;
};

ubool IS_MODULE(Value value);

ObjModule *newModule(String *name, ubool includeGlobals);
ObjModule *newModuleFromCString(const char *name, ubool includeGlobals);
void moduleAddFunctions(ObjModule *module, CFunction **functions);
void moduleRetain(ObjModule *module, Value value);
void moduleRelease(ObjModule *module, Value value);
String *moduleRetainCString(ObjModule *module, const char *value);
ObjClass *newClass(String *name);
ObjClass *newClassFromCString(const char *name);
ObjClass *newForeverClassFromCString(const char *name);
ObjClass *newClassForModule(ObjModule *module, const char *name);
ObjClass *newNativeClass(
  ObjModule *module,
  NativeObjectDescriptor *descriptor,
  CFunction **methods,
  CFunction **staticMethods);
ObjClass *newBuiltinClass(
  const char *name,
  ObjClass **slot,
  TypePatternType typePatternType,
  CFunction **methods,
  CFunction **staticMethods);
ObjClosure *newClosure(ObjThunk *function, ObjModule *module);
ObjThunk *newThunk(void);
ObjInstance *newInstance(ObjClass *klass);
ObjBuffer *newBuffer(void);
ObjBuffer *newBufferWithExternalData(Value owner, u8 *data, size_t length);
ObjList *newList(size_t size);
ObjList *newListFromArray(Value *values, size_t length);
ubool newListFromIterable(Value iterable, ObjList **out);
ObjFrozenList *copyFrozenList(Value *buffer, size_t length);
ubool newFrozenListFromIterable(Value iterable, ObjFrozenList **out);
ObjDict *newDict(void);
ubool newDictFromMap(Value map, ObjDict **out);
ubool newDictFromPairs(Value iterable, ObjDict **out);
ObjFrozenDict *newFrozenDict(Map *map);
ObjNative *newNative(NativeObjectDescriptor *descriptor, size_t objectSize);
ObjUpvalue *newUpvalue(Value *slot);
ObjClass *getClassOfValue(Value value);
ubool classHasMethod(ObjClass *cls, String *name);
void printObject(Value value);
const char *getObjectTypeName(ObjType type);

/* should-be-inline */ ubool isNative(
  Value value, NativeObjectDescriptor *descriptor);
/* should-be-inline */ ubool isObjType(Value value, ObjType type);

NativeObjectDescriptor *getNativeObjectDescriptor(Value value);

void nopBlacken(ObjNative *n);
void nopFree(ObjNative *n);

Value LIST_VAL(ObjList *list);
Value DICT_VAL(ObjDict *dict);
Value FROZEN_DICT_VAL(ObjFrozenDict *fdict);
Value INSTANCE_VAL(ObjInstance *instance);
Value MODULE_VAL(ObjModule *instance);
Value BUFFER_VAL(ObjBuffer *buffer);
Value THUNK_VAL(ObjThunk *thunk);
Value CLOSURE_VAL(ObjClosure *closure);
Value FROZEN_LIST_VAL(ObjFrozenList *frozenList);
Value CLASS_VAL(ObjClass *klass);

#endif/*mtots_object_h*/

#include "mtots_m_random.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mtots_vm.h"

static Random defaultInstance;

ObjRandom *asRandom(Value value) {
  if (!isRandom(value)) {
    panic("Expected Random but got %s", getKindName(value));
  }
  return (ObjRandom *)AS_OBJ_UNSAFE(value);
}

static Status implRandom(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(randomFloat(&defaultInstance));
  return STATUS_OK;
}

static CFunction funcRandom = {implRandom, "random"};

static Status implInstantiateRandom(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = NEW_NATIVE(ObjRandom, &descriptorRandom);
  u32 seed = argCount > 0 ? asU32(args[0]) : 19937 /* default seed */;
  initRandom(&random->handle, seed);
  *out = OBJ_VAL_EXPLICIT((Obj *)random);
  return STATUS_OK;
}

static CFunction funcInstantiateRandom = {implInstantiateRandom, "__call__", 0, 1};

NativeObjectDescriptor descriptorRandom = {
    nopBlacken,
    nopFree,
    sizeof(ObjRandom),
    "Random",
};

static Status implRandomSeed(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = asRandom(args[-1]);
  u32 seed = asU32(args[0]);
  initRandom(&random->handle, seed);
  return STATUS_OK;
}

static CFunction funcRandomSeed = {implRandomSeed, "seed", 1};

static Status implRandomNext(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = asRandom(args[-1]);
  u32 value = randomNext(&random->handle);
  *out = NUMBER_VAL(value);
  return STATUS_OK;
}

static CFunction funcRandomNext = {implRandomNext, "next"};

static Status implRandomNumber(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = asRandom(args[-1]);
  double value = randomFloat(&random->handle);
  *out = NUMBER_VAL(value);
  return STATUS_OK;
}

static CFunction funcRandomNumber = {
    implRandomNumber,
    "number",
};

static Status implRandomInt(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = asRandom(args[-1]);
  i32 low, high;
  if (argCount > 1) {
    low = asI32(args[0]);
    high = asI32(args[1]);
  } else {
    low = 0;
    high = asI32(args[0]);
  }
  if (low > high) {
    runtimeError(
        "random.int() requires low <= high, but "
        "low = %ld, high = %ld",
        (long)low, (long)high);
    return STATUS_ERROR;
  }
  *out = NUMBER_VAL(((double)low) + randomInt(&random->handle, high - low));
  return STATUS_OK;
}

static CFunction funcRandomInt = {implRandomInt, "int", 1, 2};

static Status implRandomRange(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = asRandom(args[-1]);
  i32 start = 0, end;
  if (argCount > 1) {
    start = asI32(args[0]);
    end = asI32(args[1]);
  } else {
    end = asI32(args[0]);
  }
  if (start >= end) {
    runtimeError(
        "random.range() requires start < end but got "
        "start = %ld,  end = %ld",
        (long)start, (long)end);
    return STATUS_ERROR;
  }
  *out = NUMBER_VAL(((double)start) + randomInt(&random->handle, end - start - 1));
  return STATUS_OK;
}

static CFunction funcRandomRange = {implRandomRange, "range", 1, 2};

static Status impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = asModule(args[0]);
  CFunction *functions[] = {
      &funcRandom,
      NULL,
  };
  CFunction *methods[] = {
      &funcRandomSeed,
      &funcRandomNext,
      &funcRandomNumber,
      &funcRandomInt,
      &funcRandomRange,
      NULL,
  };
  CFunction *staticMethods[] = {
      &funcInstantiateRandom,
      NULL,
  };

  initRandom(&defaultInstance, ((u32)time(NULL)) ^ (u32)rand());

  moduleAddFunctions(module, functions);

  newNativeClass(module, &descriptorRandom, methods, staticMethods);

  return STATUS_OK;
}

static CFunction func = {impl, "random", 1};

void addNativeModuleRandom(void) {
  addNativeModule(&func);
}

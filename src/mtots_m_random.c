#include "mtots_m_random.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mtots_vm.h"

static Random defaultInstance;

static ubool implRandom(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(randomFloat(&defaultInstance));
  return UTRUE;
}

static CFunction funcRandom = {implRandom, "random"};

static ubool implInstantiateRandom(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = NEW_NATIVE(ObjRandom, &descriptorRandom);
  u32 seed = argCount > 0 ? asU32(args[0]) : 19937 /* default seed */;
  initRandom(&random->handle, seed);
  *out = OBJ_VAL_EXPLICIT((Obj *)random);
  return UTRUE;
}

static CFunction funcInstantiateRandom = {
    implInstantiateRandom, "__call__", 0, 1, argsNumbers};

NativeObjectDescriptor descriptorRandom = {
    nopBlacken,
    nopFree,
    sizeof(ObjRandom),
    "Random",
};

static ubool implRandomSeed(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = AS_RANDOM(args[-1]);
  u32 seed = asU32(args[0]);
  initRandom(&random->handle, seed);
  return UTRUE;
}

static CFunction funcRandomSeed = {
    implRandomSeed,
    "seed",
    1,
    0,
    argsNumbers,
};

static ubool implRandomNext(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = AS_RANDOM(args[-1]);
  u32 value = randomNext(&random->handle);
  *out = NUMBER_VAL(value);
  return UTRUE;
}

static CFunction funcRandomNext = {implRandomNext, "next"};

static ubool implRandomNumber(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = AS_RANDOM(args[-1]);
  double value = randomFloat(&random->handle);
  *out = NUMBER_VAL(value);
  return UTRUE;
}

static CFunction funcRandomNumber = {
    implRandomNumber,
    "number",
};

static ubool implRandomInt(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = AS_RANDOM(args[-1]);
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
    return UFALSE;
  }
  *out = NUMBER_VAL(((double)low) + randomInt(&random->handle, high - low));
  return UTRUE;
}

static CFunction funcRandomInt = {implRandomInt, "int", 1, 2};

static ubool implRandomRange(i16 argCount, Value *args, Value *out) {
  ObjRandom *random = AS_RANDOM(args[-1]);
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
    return UFALSE;
  }
  *out = NUMBER_VAL(((double)start) + randomInt(&random->handle, end - start - 1));
  return UTRUE;
}

static CFunction funcRandomRange = {
    implRandomRange,
    "range",
    1,
    2,
    argsNumbers,
};

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
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

  return UTRUE;
}

static CFunction func = {impl, "random", 1};

void addNativeModuleRandom(void) {
  addNativeModule(&func);
}

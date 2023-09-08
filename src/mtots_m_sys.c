#include "mtots_m_sys.h"

#include "mtots_vm.h"

static ObjList *programArgs;

void registerArgs(int argc, const char **argv) {
  int i;
  programArgs = newList(argc);
  addForeverValue(LIST_VAL(programArgs));
  for (i = 0; i < argc; i++) {
    programArgs->buffer[i] = STRING_VAL(internCString(argv[i]));
  }
}

static ubool implGetMallocCount(i16 argc, Value *args, Value *out) {
  *out = NUMBER_VAL(vm.memory.mallocCount);
  return UTRUE;
}

static CFunction funcGetMallocCount = {implGetMallocCount, "getMallocCount"};

static ubool implEnableGCLogs(i16 argc, Value *args, Value *out) {
  vm.enableGCLogs = asBool(args[0]);
  return UTRUE;
}

static CFunction funcEnableGCLogs = {implEnableGCLogs, "enableGCLogs", 1, 0};

static ubool implEnableMallocFreeLogs(i16 argc, Value *args, Value *out) {
  vm.enableMallocFreeLogs = asBool(args[0]);
  return UTRUE;
}

static CFunction funcEnableMallocFreeLogs = {
    implEnableMallocFreeLogs, "enableMallocFreeLogs", 1, 0};

static ubool implEnableLogOnGC(i16 argc, Value *args, Value *out) {
  vm.enableLogOnGC = asBool(args[0]);
  return UTRUE;
}

static CFunction funcEnableLogOnGC = {implEnableLogOnGC, "enableLogOnGC", 1, 0};

static ubool impl(i16 argc, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
      &funcGetMallocCount,
      &funcEnableGCLogs,
      &funcEnableMallocFreeLogs,
      &funcEnableLogOnGC,
      NULL,
  };
  CFunction **function;

  mapSetN(&module->fields, "sizeOfValue", NUMBER_VAL(sizeof(Value)));

  if (programArgs) {
    mapSetN(&module->fields, "argv", LIST_VAL(programArgs));
  }

  for (function = functions; *function; function++) {
    mapSetN(&module->fields, (*function)->name, CFUNCTION_VAL(*function));
  }

  return UTRUE;
}

static CFunction func = {impl, "sys", 1};

void addNativeModuleSys(void) {
  addNativeModule(&func);
}

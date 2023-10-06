#include "mtots_m_atexit.h"

#include <stdlib.h>

#include "mtots_vm.h"

static Status implRegister(i16 argc, Value *argv, Value *out) {
  registerMtotsAtExitCallback(argv[0]);
  return STATUS_OK;
}

static CFunction funcRegister = {implRegister, "register", 1};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcRegister,
      NULL,
  };

  moduleAddFunctions(module, functions);

  return STATUS_OK;
}

static CFunction func = {impl, "atexit", 1};

void addNativeModuleAtexit(void) {
  addNativeModule(&func);
}

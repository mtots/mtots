#include "mtots_m_platform.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_m_os.h"
#include "mtots_vm.h"

static Status implSystem(i16 argc, Value *argv, Value *out) {
  *out = STRING_VAL(internCString(MTOTS_PLATFORM_SYSTEM));
  return STATUS_OK;
}

static CFunction funcSystem = {implSystem, "system"};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcSystem,
      NULL,
  };

  moduleAddFunctions(module, functions);

  return STATUS_OK;
}

static CFunction func = {impl, "platform", 1};

void addNativeModulePlatform(void) {
  addNativeModule(&func);
}

#include "mtots_m_c.h"

#include <stdlib.h>

#include "mtots.h"
#include "mtots_macros.h"

WRAP_PUBLIC_C_TYPE(IntCell, int)
WRAP_C_FUNCTION_EX(get, IntCellGet, 0, 0, *out = valNumber(asIntCell(argv[-1])->handle))
WRAP_C_FUNCTION_EX(set, IntCellSet, 0, 0, asIntCell(argv[-1])->handle = asInt(argv[0]))
static CFunction *IntCellMethods[] = {
    &funcIntCellGet,
    &funcIntCellSet,
    NULL,
};

WRAP_PUBLIC_C_TYPE(U32Cell, u32)
WRAP_C_FUNCTION_EX(get, U32CellGet, 0, 0, *out = valNumber(asU32Cell(argv[-1])->handle))
WRAP_C_FUNCTION_EX(set, U32CellSet, 0, 0, asU32Cell(argv[-1])->handle = asInt(argv[0]))
static CFunction *U32CellMethods[] = {
    &funcU32CellGet,
    &funcU32CellSet,
    NULL,
};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      NULL,
  };

  moduleAddFunctions(module, functions);
  ADD_TYPE_TO_MODULE(IntCell);
  ADD_TYPE_TO_MODULE(U32Cell);

  return STATUS_OK;
}

static CFunction func = {impl, "c", 1};

void addNativeModuleC(void) {
  addNativeModule(&func);
}

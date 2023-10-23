#include "mtots_m_c.h"

#include <stdlib.h>

#include "mtots.h"

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

static void blackenConstU8Array(ObjNative *n) {
  ObjConstU8Array *arr = (ObjConstU8Array *)n;
  markValue(arr->handle.dataOwner);
}
WRAP_PUBLIC_C_TYPE_EX(ConstU8Array, ConstU8Array, blackenConstU8Array, nopFree)
WRAP_C_FUNCTION_EX(
    __len__, ConstU8ArrayLen, 0, 0,
    *out = valNumber(asConstU8Array(argv[-1])->handle.count))
WRAP_C_FUNCTION_EX(__getitem__, ConstU8ArrayGet, 1, 0, {
  ObjConstU8Array *array = asConstU8Array(argv[-1]);
  size_t index = asIndex(argv[0], array->handle.count);
  *out = valNumber(array->handle.buffer[index]);
})
static CFunction *ConstU8ArrayMethods[] = {
    &funcConstU8ArrayLen,
    &funcConstU8ArrayGet,
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
  ADD_TYPE_TO_MODULE(ConstU8Array);

  return STATUS_OK;
}

static CFunction func = {impl, "c", 1};

void addNativeModuleC(void) {
  addNativeModule(&func);
}

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

static void blackenConstU8Slice(ObjNative *n) {
  ObjConstU8Slice *arr = (ObjConstU8Slice *)n;
  markValue(arr->handle.dataOwner);
}
WRAP_PUBLIC_C_TYPE_EX(ConstU8Slice, ConstU8Slice, blackenConstU8Slice, nopFree)
WRAP_C_FUNCTION_EX(
    __len__, ConstU8SliceLen, 0, 0,
    *out = valNumber(asConstU8Slice(argv[-1])->handle.count))
WRAP_C_FUNCTION_EX(__getitem__, ConstU8SliceGet, 1, 0, {
  ObjConstU8Slice *array = asConstU8Slice(argv[-1]);
  size_t index = asIndex(argv[0], array->handle.count);
  *out = valNumber(array->handle.buffer[index]);
})
static CFunction *ConstU8SliceMethods[] = {
    &funcConstU8SliceLen,
    &funcConstU8SliceGet,
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
  ADD_TYPE_TO_MODULE(ConstU8Slice);

  return STATUS_OK;
}

static CFunction func = {impl, "c", 1};

void addNativeModuleC(void) {
  addNativeModule(&func);
}

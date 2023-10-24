#include "mtots_m_c.h"

#include <stdlib.h>

#include "mtots.h"

#define WRAP_CONST(name, value) \
  mapSetN(&module->fields, #name, valNumber(value))

WRAP_C_FUNCTION_EX(sizeof, Sizeof, 1, 0, {
  *out = valNumber(getPointerItemSize(asPointerType(argv[0])));
})

WRAP_C_FUNCTION_EX(malloc, Malloc, 1, 0, {
  *out = valPointer(newTypedPointer(malloc(asSize(argv[0])), POINTER_TYPE_VOID));
})

WRAP_C_FUNCTION_EX(mallocSizeof, MallocSizeof, 1, 2, {
  PointerType pointerType = asPointerType(argv[0]);
  size_t count = argc > 1 && !isNil(argv[1]) ? asSize(argv[1]) : 1;
  size_t itemSize = getPointerItemSize(pointerType);
  void *pointer = malloc(itemSize * count);
  *out = valPointer(newTypedPointer(pointer, pointerType));
})

WRAP_C_FUNCTION_EX(free, Free, 1, 0, free(asVoidPointer(argv[0])))

WRAP_PUBLIC_C_TYPE(IntCell, int)
WRAP_C_FUNCTION_EX(get, IntCellGet, 0, 0, *out = valNumber(asIntCell(argv[-1])->handle))
WRAP_C_FUNCTION_EX(set, IntCellSet, 0, 0, asIntCell(argv[-1])->handle = asInt(argv[0]))
static CFunction *IntCellMethods[] = {
    &funcIntCellGet,
    &funcIntCellSet,
    NULL,
};

WRAP_PUBLIC_C_TYPE(U16Cell, u16)
WRAP_C_FUNCTION_EX(get, U16CellGet, 0, 0, *out = valNumber(asU16Cell(argv[-1])->handle))
WRAP_C_FUNCTION_EX(set, U16CellSet, 0, 0, asU16Cell(argv[-1])->handle = asInt(argv[0]))
static CFunction *U16CellMethods[] = {
    &funcU16CellGet,
    &funcU16CellSet,
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
static CFunction *ConstU8SliceStaticMethods[] = {NULL};
static CFunction *ConstU8SliceMethods[] = {
    &funcConstU8SliceLen,
    &funcConstU8SliceGet,
    NULL,
};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcSizeof,
      &funcMalloc,
      &funcMallocSizeof,
      &funcFree,
      NULL,
  };

  moduleAddFunctions(module, functions);
  ADD_TYPE_TO_MODULE(IntCell);
  ADD_TYPE_TO_MODULE(U16Cell);
  ADD_TYPE_TO_MODULE(U32Cell);
  ADD_TYPE_TO_MODULE(ConstU8Slice);

  WRAP_CONST(VOID, POINTER_TYPE_VOID);
  WRAP_CONST(CHAR, POINTER_TYPE_CHAR);
  WRAP_CONST(SHORT, POINTER_TYPE_SHORT);
  WRAP_CONST(INT, POINTER_TYPE_INT);
  WRAP_CONST(LONG, POINTER_TYPE_LONG);
  WRAP_CONST(UNSIGNED_SHORT, POINTER_TYPE_UNSIGNED_SHORT);
  WRAP_CONST(UNSIGNED_INT, POINTER_TYPE_UNSIGNED_INT);
  WRAP_CONST(UNSIGNED_LONG, POINTER_TYPE_UNSIGNED_LONG);
  WRAP_CONST(U8, POINTER_TYPE_U8);
  WRAP_CONST(U16, POINTER_TYPE_U16);
  WRAP_CONST(U32, POINTER_TYPE_U32);
  WRAP_CONST(U64, POINTER_TYPE_U64);
  WRAP_CONST(I8, POINTER_TYPE_I8);
  WRAP_CONST(I16, POINTER_TYPE_I16);
  WRAP_CONST(I32, POINTER_TYPE_I32);
  WRAP_CONST(I64, POINTER_TYPE_I64);
  WRAP_CONST(SIZE_T, POINTER_TYPE_SIZE_T);
  WRAP_CONST(PTRDIFF_T, POINTER_TYPE_PTRDIFF_T);
  WRAP_CONST(FLOAT, POINTER_TYPE_FLOAT);
  WRAP_CONST(DOUBLE, POINTER_TYPE_DOUBLE);

  return STATUS_OK;
}

static CFunction func = {impl, "c", 1};

void addNativeModuleC(void) {
  addNativeModule(&func);
}

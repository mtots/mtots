#include "mtots_m_c.h"

#include <stdlib.h>

#include "mtots.h"
/*
static const void *getConstPointer(TypedPointer *ptr) {
  return ptr->isConst ? ptr->as.constVoidPointer : ptr->as.voidPointer;
}

static double derefTypedPointer(TypedPointer *ptr, ptrdiff_t offset) {
  switch (ptr->type) {
    case POINTER_TYPE_VOID:
      panic("Cannot dereference a void pointer");
    case POINTER_TYPE_CHAR:
      return *((const char *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_SHORT:
      return *((const short *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_INT:
      return *((const int *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_LONG:
      return *((const long *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_UNSIGNED_SHORT:
      return *((const unsigned short *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_UNSIGNED_INT:
      return *((const unsigned int *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_UNSIGNED_LONG:
      return *((const unsigned long *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U8:
      return *((const u8 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U16:
      return *((const u16 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U32:
      return *((const u32 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_U64:
      return *((const u64 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I8:
      return *((const i8 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I16:
      return *((const i16 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I32:
      return *((const i32 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_I64:
      return *((const i64 *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_SIZE_T:
      return *((const size_t *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_PTRDIFF_T:
      return *((const ptrdiff_t *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_FLOAT:
      return *((const float *)getConstPointer(ptr) + offset);
    case POINTER_TYPE_DOUBLE:
      return *((const double *)getConstPointer(ptr) + offset);
  }
  panic("Invalid pointer type %d (derefTypedPointer)", ptr->type);
}

WRAP_PUBLIC_C_TYPE_EX(TypedPointer, TypedPointer, nopBlacken, nopFree)
WRAP_C_FUNCTION_EX(__getitem__, TypedPointerGetItem, 1, 0, {
  ObjTypedPointer *ptr = asTypedPointer(argv[0]);
  ptrdiff_t offset = asPtrdiff(argv[1]);
  *out = valNumber(derefTypedPointer(ptr, offset));
})
static CFunction *TypedPointerStaticMethods[] = {NULL};
static CFunction *TypedPointerMethods[] = {NULL};
*/

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
      NULL,
  };

  moduleAddFunctions(module, functions);
  ADD_TYPE_TO_MODULE(IntCell);
  ADD_TYPE_TO_MODULE(U16Cell);
  ADD_TYPE_TO_MODULE(U32Cell);
  ADD_TYPE_TO_MODULE(ConstU8Slice);

  return STATUS_OK;
}

static CFunction func = {impl, "c", 1};

void addNativeModuleC(void) {
  addNativeModule(&func);
}
/*
const char *getPointerTypeName(PointerType ptype) {
  switch (ptype) {
    case POINTER_TYPE_VOID:
      return "POINTER_TYPE_VOID";
    case POINTER_TYPE_CHAR:
      return "POINTER_TYPE_CHAR";
    case POINTER_TYPE_SHORT:
      return "POINTER_TYPE_SHORT";
    case POINTER_TYPE_INT:
      return "POINTER_TYPE_INT";
    case POINTER_TYPE_LONG:
      return "POINTER_TYPE_LONG";
    case POINTER_TYPE_UNSIGNED_SHORT:
      return "POINTER_TYPE_UNSIGNED_SHORT";
    case POINTER_TYPE_UNSIGNED_INT:
      return "POINTER_TYPE_UNSIGNED_INT";
    case POINTER_TYPE_UNSIGNED_LONG:
      return "POINTER_TYPE_UNSIGNED_LONG";
    case POINTER_TYPE_U8:
      return "POINTER_TYPE_U8";
    case POINTER_TYPE_U16:
      return "POINTER_TYPE_U16";
    case POINTER_TYPE_U32:
      return "POINTER_TYPE_U32";
    case POINTER_TYPE_U64:
      return "POINTER_TYPE_U64";
    case POINTER_TYPE_I8:
      return "POINTER_TYPE_I8";
    case POINTER_TYPE_I16:
      return "POINTER_TYPE_I16";
    case POINTER_TYPE_I32:
      return "POINTER_TYPE_I32";
    case POINTER_TYPE_I64:
      return "POINTER_TYPE_I64";
    case POINTER_TYPE_SIZE_T:
      return "POINTER_TYPE_SIZE_T";
    case POINTER_TYPE_FLOAT:
      return "POINTER_TYPE_FLOAT";
    case POINTER_TYPE_DOUBLE:
      return "POINTER_TYPE_DOUBLE";
  }
  return "Unknown-Pointer-Type";
}

size_t getPointerItemSize(PointerType ptype) {
  switch (ptype) {
    case POINTER_TYPE_VOID:
      return 0;
    case POINTER_TYPE_CHAR:
      return sizeof(char);
    case POINTER_TYPE_SHORT:
      return sizeof(short);
    case POINTER_TYPE_INT:
      return sizeof(int);
    case POINTER_TYPE_LONG:
      return sizeof(long);
    case POINTER_TYPE_UNSIGNED_SHORT:
      return sizeof(unsigned short);
    case POINTER_TYPE_UNSIGNED_INT:
      return sizeof(int);
    case POINTER_TYPE_UNSIGNED_LONG:
      return sizeof(long);
    case POINTER_TYPE_U8:
      return sizeof(u8);
    case POINTER_TYPE_U16:
      return sizeof(u16);
    case POINTER_TYPE_U32:
      return sizeof(u32);
    case POINTER_TYPE_U64:
      return sizeof(u64);
    case POINTER_TYPE_I8:
      return sizeof(i8);
    case POINTER_TYPE_I16:
      return sizeof(i16);
    case POINTER_TYPE_I32:
      return sizeof(i32);
    case POINTER_TYPE_I64:
      return sizeof(i64);
    case POINTER_TYPE_SIZE_T:
      return sizeof(size_t);
    case POINTER_TYPE_FLOAT:
      return sizeof(float);
    case POINTER_TYPE_DOUBLE:
      return sizeof(double);
  }
  return 0;
}
*/

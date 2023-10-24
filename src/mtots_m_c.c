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

#include "mtots_class_pointer.h"

#include "mtots_vm.h"

static Status implGetitem(i16 argc, Value *argv, Value *out) {
  TypedPointer pointer = asPointer(argv[-1]);
  ptrdiff_t offset = asPtrdiff(argv[0]);
  *out = valNumber(derefTypedPointer(pointer, offset));
  return STATUS_OK;
}

static CFunction funcGetitem = {implGetitem, "__getitem__", 1};

static Status implSetitem(i16 argc, Value *argv, Value *out) {
  TypedPointer pointer = asPointer(argv[-1]);
  ptrdiff_t offset = asPtrdiff(argv[0]);
  double value = asNumber(argv[1]);
  assignToTypedPointer(pointer, offset, value);
  return STATUS_OK;
}

static CFunction funcSetitem = {implSetitem, "__setitem__", 2};

static Status implAdd(i16 argc, Value *argv, Value *out) {
  TypedPointer pointer = asPointer(argv[-1]);
  ptrdiff_t offset = asPtrdiff(argv[0]);
  *out = valPointer(addToTypedPointer(pointer, offset));
  return STATUS_OK;
}

static CFunction funcAdd = {implAdd, "__add__", 1};

static Status implSub(i16 argc, Value *argv, Value *out) {
  TypedPointer p1 = asPointer(argv[-1]);
  TypedPointer p2 = asPointer(argv[0]);
  *out = valNumber(subtractFromTypedPointer(p1, p2));
  return STATUS_OK;
}

static CFunction funcSub = {implSub, "__sub__", 1};

static Status implCast(i16 argc, Value *argv, Value *out) {
  TypedPointer pointer = asPointer(argv[-1]);
  PointerType newPointerType = asPointerType(argv[0]);
  pointer.metadata.type = newPointerType;
  *out = valPointer(pointer);
  return STATUS_OK;
}

static CFunction funcCast = {implCast, "cast", 1};

static Status implIsConst(i16 argc, Value *argv, Value *out) {
  TypedPointer pointer = asPointer(argv[-1]);
  *out = valBool(pointer.metadata.isConst);
  return STATUS_OK;
}

static CFunction funcIsConst = {implIsConst, "isConst"};

static Status implGetItemType(i16 argc, Value *argv, Value *out) {
  TypedPointer pointer = asPointer(argv[-1]);
  *out = valNumber(pointer.metadata.type);
  return STATUS_OK;
}

static CFunction funcGetItemType = {implGetItemType, "getItemType"};

void initPointerClass(void) {
  CFunction *methods[] = {
      &funcGetitem,
      &funcSetitem,
      &funcAdd,
      &funcSub,
      &funcCast,
      &funcIsConst,
      &funcGetItemType,
      NULL,
  };
  newBuiltinClass(
      "Pointer",
      &vm.pointerClass,
      methods,
      NULL);
}

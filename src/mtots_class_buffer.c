#include "mtots_class_buffer.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

static Status implInstantiateBuffer(i16 argCount, Value *args, Value *out) {
  *out = valBuffer(newBuffer());
  return STATUS_OK;
}

static CFunction funcInstantiateBuffer = {
    implInstantiateBuffer,
    "__call__",
};

static Status implBufferLock(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferLock(&bo->handle);
  return STATUS_OK;
}

static CFunction funcBufferLock = {implBufferLock, "lock", 0};

static Status implBufferIsLocked(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valBool(bo->handle.isLocked);
  return STATUS_OK;
}

static CFunction funcBufferIsLocked = {implBufferIsLocked, "isLocked", 0};

static Status implBufferClear(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetLength(&bo->handle, 0);
  return STATUS_OK;
}

static CFunction funcBufferClear = {implBufferClear, "clear"};

static Status implBufferSetLength(i16 argc, Value *argv, Value *out) {
  ObjBuffer *bo = asBuffer(argv[-1]);
  size_t newSize = asSize(argv[0]);
  bufferSetLength(&bo->handle, newSize);
  return STATUS_OK;
}

static CFunction funcBufferSetLength = {implBufferSetLength, "setLength", 1};

static Status implBufferSetMinCapacity(i16 argc, Value *argv, Value *out) {
  ObjBuffer *bo = asBuffer(argv[-1]);
  size_t newCap = asSize(argv[0]);
  bufferSetMinCapacity(&bo->handle, newCap);
  return STATUS_OK;
}

static CFunction funcBufferSetMinCapacity = {implBufferSetMinCapacity, "setMinCapacity", 1};

static Status implBufferClone(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  ObjBuffer *newBuf = newBuffer();
  push(valBuffer(newBuf));
  bufferAddBytes(&newBuf->handle, bo->handle.data, bo->handle.length);
  pop(); /* newBuf */
  *out = valBuffer(newBuf);
  return STATUS_OK;
}

static CFunction funcBufferClone = {implBufferClone, "clone"};

static Status implBufferLen(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bo->handle.length);
  return STATUS_OK;
}

static CFunction funcBufferLen = {implBufferLen, "__len__"};

static Status implBufferGetitem(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  size_t i = asIndex(args[0], bo->handle.length);
  *out = valNumber(bo->handle.data[i]);
  return STATUS_OK;
}

static CFunction funcBufferGetitem = {implBufferGetitem, "__getitem__", 1};

static Status implBufferSetitem(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  size_t i = asIndex(args[0], bo->handle.length);
  u8 value = asU8(args[1]);
  bo->handle.data[i] = value;
  return STATUS_OK;
}

static CFunction funcBufferSetitem = {implBufferSetitem, "__setitem__", 2};

static Status implBufferMemset(i16 argc, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  u8 value = (u8)asU32Bits(args[0]);
  size_t start = argc > 1 && !isNil(args[1]) ? asIndex(args[1], bo->handle.length) : 0;
  size_t end = argc > 2 && !isNil(args[2]) ? asIndex(args[2], bo->handle.length) : bo->handle.length;
  if (start < end) {
    memset(bo->handle.data + start, value, end - start);
  }
  return STATUS_OK;
}

static CFunction funcBufferMemset = {implBufferMemset, "memset", 1, 3};

static Status implBufferUseLittleEndian(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bo->handle.byteOrder = MTOTS_LITTLE_ENDIAN;
  return STATUS_OK;
}

static CFunction funcBufferUseLittleEndian = {implBufferUseLittleEndian, "useLittleEndian"};

static Status implBufferUseBigEndian(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bo->handle.byteOrder = MTOTS_BIG_ENDIAN;
  return STATUS_OK;
}

static CFunction funcBufferUseBigEndian = {implBufferUseBigEndian, "useBigEndian"};

static Status implBufferView(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  ObjBuffer *newBuffer;
  size_t start = asIndex(args[0], bo->handle.length);
  size_t end = argCount > 1 && !isNil(args[1]) ? asIndexUpper(args[1], bo->handle.length) : bo->handle.length;
  if (end < start) {
    runtimeError(
        "Buffer.view() start cannot come after end "
        "(start=%lu, end=%lu, length=%lu)",
        (unsigned long)start,
        (unsigned long)end,
        (unsigned long)bo->handle.length);
    return STATUS_ERROR;
  }
  bufferLock(&bo->handle);
  newBuffer = newBufferWithExternalData(
      valBuffer(bo), bo->handle.data + start, end - start);
  newBuffer->handle.byteOrder = bo->handle.byteOrder;
  *out = valBuffer(newBuffer);
  return STATUS_OK;
}

static CFunction funcBufferView = {implBufferView, "view", 1, 2};

static Status implBufferAddI8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddI8(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddI8 = {implBufferAddI8, "addI8", 1};

static Status implBufferAddU8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddU8(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddU8 = {implBufferAddU8, "addU8", 1};

static Status implBufferAddI16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddI16(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddI16 = {implBufferAddI16, "addI16", 1};

static Status implBufferAddU16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddU16(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddU16 = {implBufferAddU16, "addU16", 1};

static Status implBufferAddI32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddI32(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddI32 = {implBufferAddI32, "addI32", 1};

static Status implBufferAddU32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddU32(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddU32 = {implBufferAddU32, "addU32", 1};

static Status implBufferAddF32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddF32(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddF32 = {implBufferAddF32, "addF32", 1};

static Status implBufferAddF64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddF64(&bo->handle, asNumber(args[0]));
  return STATUS_OK;
}

static CFunction funcBufferAddF64 = {implBufferAddF64, "addF64", 1};

static Status implBufferAddBase64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  String *string = asString(args[0]);
  if (!decodeBase64(string->chars, string->byteLength, &bo->handle)) {
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcBufferAddBase64 = {
    implBufferAddBase64,
    "addBase64",
    1,
    0,
};

static Status implBufferGetI8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetI8(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetI8 = {implBufferGetI8, "getI8", 1};

static Status implBufferGetU8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetU8(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetU8 = {implBufferGetU8, "getU8", 1};

static Status implBufferGetI16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetI16(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetI16 = {implBufferGetI16, "getI16", 1};

static Status implBufferGetU16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetU16(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetU16 = {implBufferGetU16, "getU16", 1};

static Status implBufferGetI32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetI32(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetI32 = {implBufferGetI32, "getI32", 1};

static Status implBufferGetU32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetU32(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetU32 = {implBufferGetU32, "getU32", 1};

static Status implBufferGetF32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetF32(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetF32 = {implBufferGetF32, "getF32", 1};

static Status implBufferGetF64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = valNumber(bufferGetF64(&bo->handle, asNumber(args[0])));
  return STATUS_OK;
}

static CFunction funcBufferGetF64 = {implBufferGetF64, "getF64", 1};

static Status implBufferSetI8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetI8(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetI8 = {implBufferSetI8, "setI8", 2};

static Status implBufferSetU8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetU8(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetU8 = {implBufferSetU8, "setU8", 2};

static Status implBufferSetI16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetI16(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetI16 = {implBufferSetI16, "setI16", 2};

static Status implBufferSetU16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetU16(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetU16 = {implBufferSetU16, "setU16", 2};

static Status implBufferSetI32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetI32(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetI32 = {implBufferSetI32, "setI32", 2};

static Status implBufferSetU32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetU32(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetU32 = {implBufferSetU32, "setU32", 2};

static Status implBufferSetF32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetF32(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetF32 = {implBufferSetF32, "setF32", 2};

static Status implBufferSetF64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetF64(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return STATUS_OK;
}

static CFunction funcBufferSetF64 = {implBufferSetF64, "setF64", 2};

static Status implBufferMemcpy(i16 argCount, Value *args, Value *out) {
  ObjBuffer *buffer = asBuffer(args[-1]);
  size_t dstIndex = asIndex(args[0], buffer->handle.length);
  ObjBuffer *src = asBuffer(args[1]);
  size_t start = argCount > 2 ? asIndex(args[2], src->handle.length) : 0;
  size_t end = argCount > 3 ? asIndexUpper(args[3], src->handle.length) : src->handle.length;
  if (start < end) {
    size_t len = end - start;
    if (start + len > src->handle.length) {
      len = src->handle.length - start;
    }
    bufferSetBytes(&buffer->handle, dstIndex, src->handle.data + start, len);
  }
  return STATUS_OK;
}

static CFunction funcBufferMemcpy = {implBufferMemcpy, "memcpy", 2, 4};

static Status implBufferAsString(i16 argc, Value *argv, Value *out) {
  ObjBuffer *bo = asBuffer(argv[-1]);
  String *string = bufferToString(&bo->handle);
  *out = valString(string);
  return STATUS_OK;
}

static CFunction funcBufferAsString = {implBufferAsString, "asString"};

static Status implBufferGetPointer(i16 argc, Value *argv, Value *out) {
  ObjBuffer *bo = asBuffer(argv[-1]);
  size_t i = argc > 0 && !isNil(argv[0]) ? asIndex(argv[0], bo->handle.length) : 0;
  bufferLock(&bo->handle);
  *out = valPointer(newTypedPointer(&bo->handle.data + i, POINTER_TYPE_U8));
  return STATUS_OK;
}

static CFunction funcBufferGetPointer = {implBufferGetPointer, "getPointer", 0, 1};

static Status implBufferStaticFromSize(i16 argCount, Value *args, Value *out) {
  size_t size = asU32(args[0]);
  ObjBuffer *bo = newBuffer();
  push(valBuffer(bo));
  bufferSetLength(&bo->handle, size);
  pop(); /* bo */
  *out = valBuffer(bo);
  return STATUS_OK;
}

static CFunction funcBufferStaticFromSize = {implBufferStaticFromSize, "fromSize", 1, 0};

static Status implBufferStaticFromString(i16 argCount, Value *args, Value *out) {
  String *string = asString(args[0]);
  ObjBuffer *bo = newBuffer();
  push(valBuffer(bo));
  bufferAddBytes(&bo->handle, string->chars, string->byteLength);
  pop(); /* bo */
  *out = valBuffer(bo);
  return STATUS_OK;
}

static CFunction funcBufferStaticFromString = {implBufferStaticFromString, "fromString", 1, 0};

static Status implBufferStaticFromList(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[0]);
  ObjBuffer *bo = newBuffer();
  size_t i;
  push(valBuffer(bo));
  for (i = 0; i < list->length; i++) {
    Value item = list->buffer[i];
    if (isNumber(item)) {
      u8 itemValue = asNumber(item);
      bufferAddU8(&bo->handle, itemValue);
    } else {
      runtimeError(
          "Buffer.fromList() requires a list of numbers, "
          "but found list item %s",
          getKindName(item));
      return STATUS_ERROR;
    }
  }
  pop(); /* bo */
  *out = valBuffer(bo);
  return STATUS_OK;
}

static CFunction funcBufferStaticFromList = {implBufferStaticFromList, "fromList", 1};

void initBufferClass(void) {
  CFunction *methods[] = {
      &funcBufferLock,
      &funcBufferIsLocked,
      &funcBufferClear,
      &funcBufferSetLength,
      &funcBufferSetMinCapacity,
      &funcBufferClone,
      &funcBufferLen,
      &funcBufferGetitem,
      &funcBufferSetitem,
      &funcBufferMemset,
      &funcBufferUseLittleEndian,
      &funcBufferUseBigEndian,
      &funcBufferView,
      &funcBufferAddI8,
      &funcBufferAddU8,
      &funcBufferAddI16,
      &funcBufferAddU16,
      &funcBufferAddI32,
      &funcBufferAddU32,
      &funcBufferAddF32,
      &funcBufferAddF64,
      &funcBufferAddBase64,
      &funcBufferGetI8,
      &funcBufferGetU8,
      &funcBufferGetI16,
      &funcBufferGetU16,
      &funcBufferGetI32,
      &funcBufferGetU32,
      &funcBufferGetF32,
      &funcBufferGetF64,
      &funcBufferSetI8,
      &funcBufferSetU8,
      &funcBufferSetI16,
      &funcBufferSetU16,
      &funcBufferSetI32,
      &funcBufferSetU32,
      &funcBufferSetF32,
      &funcBufferSetF64,
      &funcBufferMemcpy,
      &funcBufferAsString,
      &funcBufferGetPointer,
      NULL,
  };
  CFunction *staticMethods[] = {
      &funcBufferStaticFromSize,
      &funcBufferStaticFromString,
      &funcBufferStaticFromList,
      &funcInstantiateBuffer,
      NULL,
  };
  newBuiltinClass("Buffer", &vm.bufferClass, methods, staticMethods);
}

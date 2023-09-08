#include "mtots_class_buffer.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

static ubool implInstantiateBuffer(i16 argCount, Value *args, Value *out) {
  *out = BUFFER_VAL(newBuffer());
  return UTRUE;
}

static CFunction funcInstantiateBuffer = {
    implInstantiateBuffer,
    "__call__",
};

static ubool implBufferLock(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferLock(&bo->handle);
  return UTRUE;
}

static CFunction funcBufferLock = {implBufferLock, "lock", 0};

static ubool implBufferIsLocked(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = BOOL_VAL(bo->handle.isLocked);
  return UTRUE;
}

static CFunction funcBufferIsLocked = {implBufferIsLocked, "isLocked", 0};

static ubool implBufferClear(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetLength(&bo->handle, 0);
  return UTRUE;
}

static CFunction funcBufferClear = {implBufferClear, "clear"};

static ubool implBufferClone(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  ObjBuffer *newBuf = newBuffer();
  push(BUFFER_VAL(newBuf));
  bufferAddBytes(&newBuf->handle, bo->handle.data, bo->handle.length);
  pop(); /* newBuf */
  *out = BUFFER_VAL(newBuf);
  return UTRUE;
}

static CFunction funcBufferClone = {implBufferClone, "clone"};

static ubool implBufferLen(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bo->handle.length);
  return UTRUE;
}

static CFunction funcBufferLen = {implBufferLen, "__len__"};

static ubool implBufferGetitem(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  size_t i = asIndex(args[0], bo->handle.length);
  *out = NUMBER_VAL(bo->handle.data[i]);
  return UTRUE;
}

static CFunction funcBufferGetitem = {implBufferGetitem, "__getitem__", 1};

static ubool implBufferSetitem(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  size_t i = asIndex(args[0], bo->handle.length);
  u8 value = asU8(args[1]);
  bo->handle.data[i] = value;
  return UTRUE;
}

static CFunction funcBufferSetitem = {implBufferSetitem, "__setitem__", 2};

static ubool implBufferMemset(i16 argc, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  u8 value = (u8)asU32Bits(args[0]);
  size_t start = argc > 1 && !IS_NIL(args[1]) ? asIndex(args[1], bo->handle.length) : 0;
  size_t end = argc > 2 && !IS_NIL(args[2]) ? asIndex(args[2], bo->handle.length) : bo->handle.length;
  if (start < end) {
    memset(bo->handle.data + start, value, end - start);
  }
  return UTRUE;
}

static CFunction funcBufferMemset = {implBufferMemset, "memset", 1, 3};

static ubool implBufferUseLittleEndian(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bo->handle.byteOrder = MTOTS_LITTLE_ENDIAN;
  return UTRUE;
}

static CFunction funcBufferUseLittleEndian = {implBufferUseLittleEndian, "useLittleEndian"};

static ubool implBufferUseBigEndian(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bo->handle.byteOrder = MTOTS_BIG_ENDIAN;
  return UTRUE;
}

static CFunction funcBufferUseBigEndian = {implBufferUseBigEndian, "useBigEndian"};

static ubool implBufferView(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  ObjBuffer *newBuffer;
  size_t start = asIndex(args[0], bo->handle.length);
  size_t end = argCount > 1 && !IS_NIL(args[1]) ? asIndexUpper(args[1], bo->handle.length) : bo->handle.length;
  if (end < start) {
    runtimeError(
        "Buffer.view() start cannot come after end "
        "(start=%lu, end=%lu, length=%lu)",
        (unsigned long)start,
        (unsigned long)end,
        (unsigned long)bo->handle.length);
    return UFALSE;
  }
  bufferLock(&bo->handle);
  newBuffer = newBufferWithExternalData(
      BUFFER_VAL(bo), bo->handle.data + start, end - start);
  newBuffer->handle.byteOrder = bo->handle.byteOrder;
  *out = BUFFER_VAL(newBuffer);
  return UTRUE;
}

static CFunction funcBufferView = {implBufferView, "view", 1, 2};

static ubool implBufferAddI8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddI8(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddI8 = {implBufferAddI8, "addI8", 1};

static ubool implBufferAddU8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddU8(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddU8 = {implBufferAddU8, "addU8", 1};

static ubool implBufferAddI16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddI16(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddI16 = {implBufferAddI16, "addI16", 1};

static ubool implBufferAddU16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddU16(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddU16 = {implBufferAddU16, "addU16", 1};

static ubool implBufferAddI32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddI32(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddI32 = {implBufferAddI32, "addI32", 1};

static ubool implBufferAddU32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddU32(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddU32 = {implBufferAddU32, "addU32", 1};

static ubool implBufferAddF32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddF32(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddF32 = {implBufferAddF32, "addF32", 1};

static ubool implBufferAddF64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferAddF64(&bo->handle, asNumber(args[0]));
  return UTRUE;
}

static CFunction funcBufferAddF64 = {implBufferAddF64, "addF64", 1};

static ubool implBufferAddBase64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  String *string = asString(args[0]);
  if (!decodeBase64(string->chars, string->byteLength, &bo->handle)) {
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcBufferAddBase64 = {
    implBufferAddBase64,
    "addBase64",
    1,
    0,
};

static ubool implBufferGetI8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetI8(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetI8 = {implBufferGetI8, "getI8", 1};

static ubool implBufferGetU8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetU8(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetU8 = {implBufferGetU8, "getU8", 1};

static ubool implBufferGetI16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetI16(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetI16 = {implBufferGetI16, "getI16", 1};

static ubool implBufferGetU16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetU16(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetU16 = {implBufferGetU16, "getU16", 1};

static ubool implBufferGetI32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetI32(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetI32 = {implBufferGetI32, "getI32", 1};

static ubool implBufferGetU32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetU32(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetU32 = {implBufferGetU32, "getU32", 1};

static ubool implBufferGetF32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetF32(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetF32 = {implBufferGetF32, "getF32", 1};

static ubool implBufferGetF64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  *out = NUMBER_VAL(bufferGetF64(&bo->handle, asNumber(args[0])));
  return UTRUE;
}

static CFunction funcBufferGetF64 = {implBufferGetF64, "getF64", 1};

static ubool implBufferSetI8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetI8(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetI8 = {implBufferSetI8, "setI8", 2};

static ubool implBufferSetU8(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetU8(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetU8 = {implBufferSetU8, "setU8", 2};

static ubool implBufferSetI16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetI16(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetI16 = {implBufferSetI16, "setI16", 2};

static ubool implBufferSetU16(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetU16(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetU16 = {implBufferSetU16, "setU16", 2};

static ubool implBufferSetI32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetI32(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetI32 = {implBufferSetI32, "setI32", 2};

static ubool implBufferSetU32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetU32(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetU32 = {implBufferSetU32, "setU32", 2};

static ubool implBufferSetF32(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetF32(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetF32 = {implBufferSetF32, "setF32", 2};

static ubool implBufferSetF64(i16 argCount, Value *args, Value *out) {
  ObjBuffer *bo = asBuffer(args[-1]);
  bufferSetF64(&bo->handle, asNumber(args[0]), asNumber(args[1]));
  return UTRUE;
}

static CFunction funcBufferSetF64 = {implBufferSetF64, "setF64", 2};

static ubool implBufferMemcpy(i16 argCount, Value *args, Value *out) {
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
  return UTRUE;
}

static CFunction funcBufferMemcpy = {implBufferMemcpy, "memcpy", 2, 4};

static ubool implBufferStaticFromSize(i16 argCount, Value *args, Value *out) {
  size_t size = asU32(args[0]);
  ObjBuffer *bo = newBuffer();
  push(BUFFER_VAL(bo));
  bufferSetLength(&bo->handle, size);
  pop(); /* bo */
  *out = BUFFER_VAL(bo);
  return UTRUE;
}

static CFunction funcBufferStaticFromSize = {implBufferStaticFromSize, "fromSize", 1, 0};

static ubool implBufferStaticFromString(i16 argCount, Value *args, Value *out) {
  String *string = asString(args[0]);
  ObjBuffer *bo = newBuffer();
  push(BUFFER_VAL(bo));
  bufferAddBytes(&bo->handle, string->chars, string->byteLength);
  pop(); /* bo */
  *out = BUFFER_VAL(bo);
  return UTRUE;
}

static CFunction funcBufferStaticFromString = {implBufferStaticFromString, "fromString", 1, 0};

static ubool implBufferStaticFromList(i16 argCount, Value *args, Value *out) {
  ObjList *list = asList(args[0]);
  ObjBuffer *bo = newBuffer();
  size_t i;
  push(BUFFER_VAL(bo));
  for (i = 0; i < list->length; i++) {
    Value item = list->buffer[i];
    if (IS_NUMBER(item)) {
      u8 itemValue = asNumber(item);
      bufferAddU8(&bo->handle, itemValue);
    } else {
      runtimeError(
          "Buffer.fromList() requires a list of numbers, "
          "but found list item %s",
          getKindName(item));
      return UFALSE;
    }
  }
  pop(); /* bo */
  *out = BUFFER_VAL(bo);
  return UTRUE;
}

static CFunction funcBufferStaticFromList = {implBufferStaticFromList, "fromList", 1};

void initBufferClass(void) {
  CFunction *methods[] = {
      &funcBufferLock,
      &funcBufferIsLocked,
      &funcBufferClear,
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

#include "mtots_m_bmon.h"

#include <math.h>
#include <string.h>

#include "mtots_vm.h"

/* When relevant, all integer are assumed to be little-endian */

#define TAG_NIL 1
#define TAG_TRUE 2
#define TAG_FALSE 3
#define TAG_NUMBER 4 /* IEEE 754 double precision, little endian */
#define TAG_STRING 5
#define TAG_LIST 6
#define TAG_DICT 7

static String *stringBmon;

static ubool runtimeErrorUnexpectedEOF(size_t i, size_t len, size_t limit) {
  runtimeError(
      "Unexpected EOF when loading BMON (i=%lu len=%lu limit=%lu)",
      (unsigned long)i,
      (unsigned long)len,
      (unsigned long)limit);
  return STATUS_ERROR;
}

static ubool load(u8 *buffer, size_t limit, size_t *pos, Value *out) {
  u8 header;
  size_t i = *pos;
  if (i + 1 > limit) {
    return runtimeErrorUnexpectedEOF(i, 1, limit);
  }
  header = buffer[i++];
  switch (header) {
    case TAG_NIL:
      *out = valNil();
      break;
    case TAG_TRUE:
      *out = valBool(1);
      break;
    case TAG_FALSE:
      *out = valBool(0);
      break;
    case TAG_NUMBER: {
      /* NOTE: type-punning double is checked for in assumptions.c. */
      union {
        double x;
        u8 arr[8];
      } u;
      if (i + 8 > limit) {
        return runtimeErrorUnexpectedEOF(i, 8, limit);
      }
      memcpy(u.arr, buffer + i, 8);
      i += 8;
      *out = valNumber(u.x);
      break;
    }
    case TAG_STRING: {
      union {
        u32 len;
        u8 arr[4];
      } u;
      size_t len;
      if (i + 4 > limit) {
        return runtimeErrorUnexpectedEOF(i, 4, limit);
      }
      memcpy(u.arr, buffer + i, 4);
      i += 4;
      len = (size_t)u.len;
      if (i + len > limit) {
        return runtimeErrorUnexpectedEOF(i, len, limit);
      }
      *out = valString(internString((const char *)(buffer + i), len));
      i += len;
      break;
    }
    case TAG_LIST: {
      ObjList *list;
      union {
        u32 len;
        u8 arr[4];
      } u;
      size_t j, len;
      if (i + 4 > limit) {
        return runtimeErrorUnexpectedEOF(i, 4, limit);
      }
      memcpy(u.arr, buffer + i, 4);
      i += 4;
      len = (size_t)u.len;
      list = newList(len);
      push(valList(list));
      for (j = 0; j < len; j++) {
        if (!load(buffer, limit, &i, list->buffer + j)) {
          return STATUS_ERROR;
        }
      }
      pop(); /* list */
      *out = valList(list);
      break;
    }
    case TAG_DICT: {
      ObjDict *dict;
      union {
        u32 len;
        u8 arr[4];
      } u;
      size_t j, len;
      if (i + 4 > limit) {
        return runtimeErrorUnexpectedEOF(i, 4, limit);
      }
      memcpy(u.arr, buffer + i, 4);
      i += 4;
      len = (size_t)u.len;
      dict = newDict();
      push(valDict(dict));
      for (j = 0; j < len; j++) {
        Value key, value;
        if (!load(buffer, limit, &i, &key)) {
          return STATUS_ERROR;
        }
        push(key);
        if (!load(buffer, limit, &i, &value)) {
          return STATUS_ERROR;
        }
        push(value);
        mapSet(&dict->map, key, value);
        pop(); /* value */
        pop(); /* key */
      }
      pop(); /* dict */
      *out = valDict(dict);
      break;
    }
    default:
      runtimeError("Invalid BMON tag %d", header);
      return STATUS_ERROR;
  }
  *pos = i;
  return STATUS_OK;
}

static Status implLoads(i16 argCount, Value *args, Value *out) {
  ObjBuffer *buffer = asBuffer(args[0]);
  size_t pos = 0;
  if (!load(buffer->handle.data, buffer->handle.length, &pos, out)) {
    return STATUS_ERROR;
  }
  if (pos < buffer->handle.length) {
    runtimeError(
        "Extra data when loading BMON (pos=%lu, length=%lu)",
        (unsigned long)pos,
        (unsigned long)buffer->handle.length);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcLoads = {implLoads, "loads", 1};

ubool bmonDump(Value value, Buffer *out) {
  switch (value.type) {
    case VAL_NIL:
      bufferAddU8(out, TAG_NIL);
      return STATUS_OK;
    case VAL_BOOL:
      bufferAddU8(out, value.as.boolean ? TAG_TRUE : TAG_FALSE);
      return STATUS_OK;
    case VAL_NUMBER: {
      union {
        double x;
        u32 arr[2];
      } u;
      u.x = asNumber(value);
      bufferAddU8(out, TAG_NUMBER);
      bufferAddU32(out, u.arr[0]);
      bufferAddU32(out, u.arr[1]);
      return STATUS_OK;
    }
    case VAL_STRING: {
      String *string = (String *)value.as.obj;
      bufferAddU8(out, TAG_STRING);
      bufferAddU32(out, string->byteLength);
      bufferAddBytes(out, string->chars, string->byteLength);
      return STATUS_OK;
    }
    case VAL_OBJ: {
      Obj *obj = AS_OBJ_UNSAFE(value);
      switch (obj->type) {
        case OBJ_LIST: {
          ObjList *list = AS_LIST_UNSAFE(value);
          u32 i, len;
          if (list->length > (size_t)U32_MAX) {
            runtimeError(
                "list is too long to serialize in BMON (length=%lu)",
                (unsigned long)list->length);
            return STATUS_ERROR;
          }
          len = (u32)list->length;
          bufferAddU8(out, TAG_LIST);
          bufferAddU32(out, list->length);
          for (i = 0; i < len; i++) {
            if (!bmonDump(list->buffer[i], out)) {
              return STATUS_ERROR;
            }
          }
          return STATUS_OK;
        }
        case OBJ_DICT: {
          ObjDict *dict = AS_DICT_UNSAFE(value);
          MapIterator it;
          MapEntry *entry;
          size_t count = 0, declaredSize = dict->map.size;
          bufferAddU8(out, TAG_DICT);
          bufferAddU32(out, declaredSize);
          initMapIterator(&it, &dict->map);
          while (mapIteratorNext(&it, &entry)) {
            if (!bmonDump(entry->key, out)) {
              return STATUS_ERROR;
            }
            if (!bmonDump(entry->value, out)) {
              return STATUS_ERROR;
            }
            count++;
          }
          if (count != declaredSize) {
            panic("Dict declared size does not match iterated size");
          }
          return STATUS_OK;
        }
        case OBJ_NATIVE:
        case OBJ_INSTANCE: {
          /* For instances, if there is a method __bmon__, use that
           * allow for a sort of custom serialization */
          ObjClass *cls = getClassOfValue(value);
          if (mapContainsKey(&cls->methods, valString(stringBmon))) {
            push(value);
            if (!callMethod(stringBmon, 0)) {
              return STATUS_ERROR;
            }
            if (!bmonDump(vm.stackTop[-1], out)) {
              return STATUS_ERROR;
            }
            pop(); /* the proxy value we just serialized */
            return STATUS_OK;
          }
        }
        default:
          break;
      }
    }
    default:
      break;
  }
  runtimeError("Cannot serialize %s to BMON", getKindName(value));
  return STATUS_ERROR;
}

static Status implDumps(i16 argCount, Value *args, Value *out) {
  Value value = args[0];
  ObjBuffer *buffer = newBuffer();
  push(valBuffer(buffer));
  if (!bmonDump(value, &buffer->handle)) {
    return STATUS_ERROR;
  }
  pop(); /* buffer */
  *out = valBuffer(buffer);
  return STATUS_OK;
}

static CFunction funcDumps = {
    implDumps, "dumps", 1};

static Status impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = asModule(args[0]);
  CFunction *functions[] = {
      &funcLoads,
      &funcDumps,
  };
  size_t i;

  stringBmon = moduleRetainCString(module, "__bmon__");

  for (i = 0; i < sizeof(functions) / sizeof(CFunction *); i++) {
    mapSetN(&module->fields, functions[i]->name, valCFunction(functions[i]));
  }

  return STATUS_OK;
}

static CFunction func = {impl, "bmon", 1};

void addNativeModuleBmon(void) {
  addNativeModule(&func);
}

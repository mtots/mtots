#include "mtots_m_bmon.h"

#include "mtots_vm.h"

#include <math.h>
#include <string.h>

/* When relevant, all integer are assumed to be little-endian */

#define TAG_NIL         1
#define TAG_TRUE        2
#define TAG_FALSE       3
#define TAG_NUMBER      4 /* IEEE 754 double precision, little endian */
#define TAG_STRING      5
#define TAG_LIST        6
#define TAG_DICT        7

static String *stringBmon;

static ubool runtimeErrorUnexpectedEOF(size_t i, size_t len, size_t limit) {
  runtimeError(
    "Unexpected EOF when loading BMON (i=%lu len=%lu limit=%lu)",
    (unsigned long)i,
    (unsigned long)len,
    (unsigned long)limit);
  return UFALSE;
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
      *out = NIL_VAL();
      break;
    case TAG_TRUE:
      *out = BOOL_VAL(1);
      break;
    case TAG_FALSE:
      *out = BOOL_VAL(0);
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
      *out = NUMBER_VAL(u.x);
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
      *out = STRING_VAL(internString((const char *)(buffer + i), len));
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
      push(LIST_VAL(list));
      for (j = 0; j < len; j++) {
        if (!load(buffer, limit, &i, list->buffer + j)) {
          return UFALSE;
        }
      }
      pop(); /* list */
      *out = LIST_VAL(list);
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
      push(DICT_VAL(dict));
      for (j = 0; j < len; j++) {
        Value key, value;
        if (!load(buffer, limit, &i, &key)) {
          return UFALSE;
        }
        push(key);
        if (!load(buffer, limit, &i, &value)) {
          return UFALSE;
        }
        push(value);
        mapSet(&dict->map, key, value);
        pop(); /* value */
        pop(); /* key */
      }
      pop(); /* dict */
      *out = DICT_VAL(dict);
      break;
    }
    default:
      runtimeError("Invalid BMON tag %d", header);
      return UFALSE;
  }
  *pos = i;
  return UTRUE;
}

static ubool implLoads(i16 argCount, Value *args, Value *out) {
  ObjBuffer *buffer = AS_BUFFER(args[0]);
  size_t pos = 0;
  if (!load(buffer->handle.data, buffer->handle.length, &pos, out)) {
    return UFALSE;
  }
  if (pos < buffer->handle.length) {
    runtimeError(
      "Extra data when loading BMON (pos=%lu, length=%lu)",
      (unsigned long)pos,
      (unsigned long)buffer->handle.length);
    return UFALSE;
  }
  return UTRUE;
}

static TypePattern argsLoads[] = {
  { TYPE_PATTERN_BUFFER },
};

static CFunction funcLoads = {
  implLoads, "loads", 1, 0, argsLoads
};

ubool bmonDump(Value value, Buffer *out) {
  switch (value.type) {
    case VAL_NIL:
      bufferAddU8(out, TAG_NIL);
      return UTRUE;
    case VAL_BOOL:
      bufferAddU8(out, AS_BOOL(value) ? TAG_TRUE : TAG_FALSE);
      return UTRUE;
    case VAL_NUMBER: {
      union {
        double x;
        u32 arr[2];
      } u;
      u.x = AS_NUMBER(value);
      bufferAddU8(out, TAG_NUMBER);
      bufferAddU32(out, u.arr[0]);
      bufferAddU32(out, u.arr[1]);
      return UTRUE;
    }
    case VAL_STRING: {
      String *string = AS_STRING(value);
      bufferAddU8(out, TAG_STRING);
      bufferAddU32(out, string->byteLength);
      bufferAddBytes(out, string->chars, string->byteLength);
      return UTRUE;
    }
    case VAL_OBJ: {
      Obj *obj = AS_OBJ(value);
      switch (obj->type) {
        case OBJ_LIST: {
          ObjList *list = AS_LIST(value);
          u32 i, len;
          if (list->length > (size_t)U32_MAX) {
            runtimeError(
              "list is too long to serialize in BMON (length=%lu)",
              (unsigned long)list->length);
            return UFALSE;
          }
          len = (u32)list->length;
          bufferAddU8(out, TAG_LIST);
          bufferAddU32(out, list->length);
          for (i = 0; i < len; i++) {
            if (!bmonDump(list->buffer[i], out)) {
              return UFALSE;
            }
          }
          return UTRUE;
        }
        case OBJ_DICT: {
          ObjDict *dict = AS_DICT(value);
          MapIterator it;
          MapEntry *entry;
          size_t count = 0, declaredSize = dict->map.size;
          bufferAddU8(out, TAG_DICT);
          bufferAddU32(out, declaredSize);
          initMapIterator(&it, &dict->map);
          while (mapIteratorNext(&it, &entry)) {
            if (!bmonDump(entry->key, out)) {
              return UFALSE;
            }
            if (!bmonDump(entry->value, out)) {
              return UFALSE;
            }
            count++;
          }
          if (count != declaredSize) {
            panic("Dict declared size does not match iterated size");
          }
          return UTRUE;
        }
        case OBJ_NATIVE:
        case OBJ_INSTANCE: {
          /* For instances, if there is a method __bmon__, use that
           * allow for a sort of custom serialization */
          ObjClass *cls = getClassOfValue(value);
          if (mapContainsKey(&cls->methods, STRING_VAL(stringBmon))) {
            push(value);
            if (!callMethod(stringBmon, 0)) {
              return UFALSE;
            }
            if (!bmonDump(vm.stackTop[-1], out)) {
              return UFALSE;
            }
            pop(); /* the proxy value we just serialized */
            return UTRUE;
          }
        }
        default: break;
      }
    }
    default: break;
  }
  runtimeError("Cannot serialize %s to BMON", getKindName(value));
  return UFALSE;
}

static ubool implDumps(i16 argCount, Value *args, Value *out) {
  Value value = args[0];
  ObjBuffer *buffer = newBuffer();
  push(BUFFER_VAL(buffer));
  if (!bmonDump(value, &buffer->handle)) {
    return UFALSE;
  }
  pop(); /* buffer */
  *out = BUFFER_VAL(buffer);
  return UTRUE;
}

static CFunction funcDumps = {
  implDumps, "dumps", 1
};

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *functions[] = {
    &funcLoads,
    &funcDumps,
  };
  size_t i;

  stringBmon = moduleRetainCString(module, "__bmon__");

  for (i = 0; i < sizeof(functions)/sizeof(CFunction*); i++) {
    mapSetN(&module->fields, functions[i]->name, CFUNCTION_VAL(functions[i]));
  }

  return UTRUE;
}

static CFunction func = { impl, "bmon", 1 };

void addNativeModuleBmon() {
  addNativeModule(&func);
}

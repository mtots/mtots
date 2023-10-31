#include <string.h>

#include "mtots_vm.h"

#define DICT_MAX_LOAD 0.75

void initMap(Map *map) {
  map->occupied = 0;
  map->capacity = 0;
  map->size = 0;
  map->entries = NULL;
  map->first = NULL;
  map->last = NULL;
}

void freeMap(Map *map) {
  FREE_ARRAY(MapEntry, map->entries, map->capacity);
  initMap(map);
}

static u32 hashNumber(double x) {
  union {
    double number;
    u32 parts[2];
  } pun;
  i32 ix = (i32)x;
  if (x == (double)ix) {
    return (u32)ix;
  }
  pun.number = x;
  /* TODO: smarter hashing */
  return pun.parts[0] ^ pun.parts[1];
}

static u32 hashVector(Vector vector) {
  /* FNV-1a as presented in the Crafting Interpreters book */
  size_t i;
  u32 hash = 2166136261u;
  u32 hashes[3];
  hashes[0] = hashNumber(vector.x);
  hashes[1] = hashNumber(vector.y);
  hashes[2] = hashNumber(vector.z);
  for (i = 0; i < 3; i++) {
    u32 itemhash = hashes[i];
    hash ^= (u8)(itemhash);
    hash *= 16777619;
    hash ^= (u8)(itemhash >> 8);
    hash *= 16777619;
    hash ^= (u8)(itemhash >> 16);
    hash *= 16777619;
    hash ^= (u8)(itemhash >> 24);
    hash *= 16777619;
  }
  return hash;
}

u32 hashval(Value value) {
  switch (value.type) {
    /* hash values for bool taken from Java */
    case VAL_NIL:
      return 17;
    case VAL_BOOL:
      return value.as.boolean ? 1231 : 1237;
    case VAL_NUMBER:
      return hashNumber(value.as.number);
    case VAL_STRING:
      return value.as.string->hash;
    case VAL_CFUNCTION:
      break;
    case VAL_SENTINEL:
      return (u32)value.as.sentinel;
    case VAL_RANGE:
      break;
    case VAL_RANGE_ITERATOR:
      break;
    case VAL_VECTOR:
      return hashVector(asVector(value));
    case VAL_POINTER:
      /* TODO: come up with better hashing */
      return (u32)(size_t)(value.extra.tpm.isConst
                               ? value.as.constVoidPointer
                               : value.as.voidPointer);
    case VAL_FILE_DESCRIPTOR:
      break;
    case VAL_OBJ:
      switch (AS_OBJ_UNSAFE(value)->type) {
        case OBJ_FROZEN_LIST:
          return AS_FROZEN_LIST_UNSAFE(value)->hash;
        case OBJ_FROZEN_DICT:
          return AS_FROZEN_DICT_UNSAFE(value)->hash;
        default:
          break;
      }
  }
  panic("%s values are not hashable", getKindName(value));
}

/* NOTE: capacity should always be non-zero.
 * If findMapEntry was a non-static function, I probably would do the check
 * in the function itself. But since it is static, all places where
 * it can be called are within this file.
 */
static MapEntry *findMapEntry(MapEntry *entries, size_t capacity, Value key) {
  /* OPT: key->hash % capacity */
  u32 index = hashval(key) & (capacity - 1);
  MapEntry *tombstone = NULL;
  for (;;) {
    MapEntry *entry = &entries[index];
    if (isEmptyKey(entry->key)) {
      if (isNil(entry->value)) {
        /* Empty entry */
        return tombstone != NULL ? tombstone : entry;
      } else if (tombstone == NULL) {
        /* We found a tombstone */
        tombstone = entry;
      }
    } else if (valuesEqual(entry->key, key)) {
      /* We found the key */
      return entry;
    }
    /* OPT: (index + 1) % capacity */
    index = (index + 1) & (capacity - 1);
  }
}

ubool mapContainsKey(Map *map, Value key) {
  MapEntry *entry;

  if (map->occupied == 0) {
    return UFALSE;
  }

  entry = findMapEntry(map->entries, map->capacity, key);
  return !isEmptyKey(entry->key);
}

ubool mapGet(Map *map, Value key, Value *value) {
  MapEntry *entry;

  if (map->occupied == 0) {
    return UFALSE;
  }

  entry = findMapEntry(map->entries, map->capacity, key);
  if (isEmptyKey(entry->key)) {
    return UFALSE;
  }

  *value = entry->value;
  return UTRUE;
}

ubool mapGetStr(Map *map, String *key, Value *value) {
  return mapGet(map, valString(key), value);
}

static void adjustMapCapacity(Map *map, size_t capacity) {
  size_t i;
  MapEntry *entries = ALLOCATE(MapEntry, capacity);
  MapEntry *p, *first = NULL, *last = NULL;
  for (i = 0; i < capacity; i++) {
    entries[i].key = valEmptyKey();
    entries[i].value = valNil();
  }

  map->occupied = 0;
  for (p = map->first; p; p = p->next) {
    MapEntry *dest;

    dest = findMapEntry(entries, capacity, p->key);
    dest->key = p->key;
    dest->value = p->value;

    if (first == NULL) {
      dest->prev = NULL;
      dest->next = NULL;
      first = last = dest;
    } else {
      dest->prev = last;
      dest->next = NULL;
      last->next = dest;
      last = dest;
    }

    map->occupied++;
  }

  FREE_ARRAY(MapEntry, map->entries, map->capacity);
  map->entries = entries;
  map->capacity = capacity;
  map->first = first;
  map->last = last;
}

ubool mapSet(Map *map, Value key, Value value) {
  MapEntry *entry;
  ubool isNewKey;

  if (map->occupied + 1 > map->capacity * DICT_MAX_LOAD) {
    size_t capacity = GROW_CAPACITY(map->capacity);
    adjustMapCapacity(map, capacity);
  }
  entry = findMapEntry(map->entries, map->capacity, key);
  isNewKey = isEmptyKey(entry->key);

  if (isNewKey) {
    /* If entry->value is not nil, we're reusing a tombstone
     * so we don't want to increment the count since tombstones
     * are already included in count.
     * We include tombstones in the count so that the loadfactor
     * is sensitive to slots occupied by tombstones */
    if (isNil(entry->value)) {
      map->occupied++;
    }
    map->size++;

    if (map->last == NULL) {
      map->first = map->last = entry;
      entry->prev = entry->next = NULL;
    } else {
      entry->prev = map->last;
      entry->next = NULL;
      map->last->next = entry;
      map->last = entry;
    }
  }
  entry->key = key;
  entry->value = value;
  return isNewKey;
}

ubool mapSetStr(Map *map, String *key, Value value) {
  return mapSet(map, valString(key), value);
}

/**
 * "mapSet with Name"
 *
 * Like mapSet, but a version that's more convenient for use in
 * native code in two ways:
 *   1) the key parameter is a C-string that is automatically be converted
 *      to an String and properly retained and released using the stack
 *   2) the value parameter will retained and popped from the stack, so that
 *      even if mapSet or copyCString triggers a reallocation, the value
 *      will not be collected.
 */
ubool mapSetN(Map *map, const char *key, Value value) {
  ubool result;
  String *keystr;

  push(value);
  keystr = internCString(key);
  push(valString(keystr));
  result = mapSet(map, valString(keystr), value);
  pop(); /* keystr */
  pop(); /* value */
  return result;
}

ubool mapDelete(Map *map, Value key) {
  MapEntry *entry;

  if (map->occupied == 0) {
    return STATUS_ERROR;
  }

  entry = findMapEntry(map->entries, map->capacity, key);
  if (isEmptyKey(entry->key)) {
    return STATUS_ERROR;
  }

  /* Update linked list */
  if (entry->prev == NULL) {
    map->first = entry->next;
  } else {
    entry->prev->next = entry->next;
  }
  if (entry->next == NULL) {
    map->last = entry->prev;
  } else {
    entry->next->prev = entry->prev;
  }
  entry->prev = entry->next = NULL;

  /* Place a tombstone in the entry */
  entry->key = valEmptyKey();
  entry->value = valBool(1);
  map->size--;
  return STATUS_OK;
}

ubool mapDeleteStr(Map *map, String *key) {
  return mapDelete(map, valString(key));
}

void mapAddAll(Map *from, Map *to) {
  size_t i;
  for (i = 0; i < from->capacity; i++) {
    MapEntry *entry = &from->entries[i];
    if (!isEmptyKey(entry->key)) {
      mapSet(to, entry->key, entry->value);
    }
  }
}

ObjFrozenList *mapFindFrozenList(
    Map *map,
    Value *buffer,
    size_t length,
    u32 hash) {
  u32 index;
  if (map->occupied == 0) {
    return NULL;
  }
  /* OPT: hash % map->capacity */
  index = hash & (map->capacity - 1);
  for (;;) {
    MapEntry *entry = &map->entries[index];
    if (isEmptyKey(entry->key)) {
      /* Stop if we find an empty non-tombstone entry */
      if (isNil(entry->value)) {
        return NULL;
      }
    } else if (isFrozenList(entry->key)) {
      ObjFrozenList *key = AS_FROZEN_LIST_UNSAFE(entry->key);
      if (key->length == length && key->hash == hash) {
        size_t i;
        ubool equal = UTRUE;
        for (i = 0; i < length; i++) {
          if (!valuesEqual(key->buffer[i], buffer[i])) {
            equal = UFALSE;
            break;
          }
        }
        if (equal) {
          return key; /* We found it */
        }
      }
    }
    /* OPT: (index + 1) % map->capacity */
    index = (index + 1) & (map->capacity - 1);
  }
}

ObjFrozenDict *mapFindFrozenDict(
    Map *map,
    Map *frozenDictMap,
    u32 hash) {
  u32 index;
  if (map->occupied == 0) {
    return NULL;
  }
  /* OPT: hash % map->capacity */
  index = hash & (map->capacity - 1);
  for (;;) {
    MapEntry *entry = &map->entries[index];
    if (isEmptyKey(entry->key)) {
      /* Stop if we find an empty non-tombstone entry */
      if (isNil(entry->value)) {
        return NULL;
      }
    } else if (isFrozenDict(entry->key)) {
      ObjFrozenDict *key = AS_FROZEN_DICT_UNSAFE(entry->key);
      if (key->hash == hash && mapsEqual(frozenDictMap, &key->map)) {
        return key; /* We found it */
      }
    }
    /* OPT: (index + 1) % map->capacity */
    index = (index + 1) & (map->capacity - 1);
  }
}

void mapRemoveWhite(Map *map) {
  size_t i;
  for (i = 0; i < map->capacity; i++) {
    MapEntry *entry = &map->entries[i];
    if (!isEmptyKey(entry->key) &&
        isObj(entry->key) &&
        !AS_OBJ_UNSAFE(entry->key)->isMarked) {
      mapDelete(map, entry->key);
    }
  }
}

void markMap(Map *map) {
  size_t i;
  for (i = 0; i < map->capacity; i++) {
    MapEntry *entry = &map->entries[i];
    if (!isEmptyKey(entry->key)) {
      markValue(entry->key);
      markValue(entry->value);
    }
  }
}

void initMapIterator(MapIterator *di, Map *map) {
  di->entry = map->first;
}

/*
ubool mapIteratorDone(MapIterator *di) {
  return di->entry == NULL;
}
*/

ubool mapIteratorNext(MapIterator *di, MapEntry **out) {
  if (di->entry) {
    *out = di->entry;
    di->entry = di->entry->next;
    return STATUS_OK;
  }
  return STATUS_ERROR;
}

ubool mapIteratorNextKey(MapIterator *di, Value *out) {
  if (di->entry) {
    *out = di->entry->key;
    di->entry = di->entry->next;
    return STATUS_OK;
  }
  return STATUS_ERROR;
}

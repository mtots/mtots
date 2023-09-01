#include "mtots_vm.h"

#include <string.h>

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

u32 hashval(Value value) {
  switch (value.type) {
    /* hash values for bool taken from Java */
    case VAL_NIL: return 17;
    case VAL_BOOL: return AS_BOOL(value) ? 1231 : 1237;
    case VAL_NUMBER: {
      double x = AS_NUMBER(value);
      union {
        double number;
        u32 parts[2];
      } pun;
      i32 ix = (i32) x;
      if (x == (double)ix) {
        return (u32) ix;
      }
      pun.number = x;
      /* TODO: smarter hashing */
      return pun.parts[0] ^ pun.parts[1];
    }
    case VAL_SYMBOL: return getSymbolHash(value.as.symbol);
    case VAL_STRING: return AS_STRING(value)->hash;
    case VAL_BUILTIN: break;
    case VAL_CFUNCTION: break;
    case VAL_SENTINEL: return (u32) AS_SENTINEL(value);
    case VAL_OBJ: switch (AS_OBJ(value)->type) {
      case OBJ_FROZEN_LIST: return AS_FROZEN_LIST(value)->hash;
      case OBJ_FROZEN_DICT: return AS_FROZEN_DICT(value)->hash;
      default: break;
    }
  }
  panic("%s values are not hashable", getKindName(value));
  return 0;
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
    if (IS_EMPTY_KEY(entry->key)) {
      if (IS_NIL(entry->value)) {
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
  return !IS_EMPTY_KEY(entry->key);
}

ubool mapGet(Map *map, Value key, Value *value) {
  MapEntry *entry;

  if (map->occupied == 0) {
    return UFALSE;
  }

  entry = findMapEntry(map->entries, map->capacity, key);
  if (IS_EMPTY_KEY(entry->key)) {
    return UFALSE;
  }

  *value = entry->value;
  return UTRUE;
}

ubool mapGetStr(Map *map, String *key, Value *value) {
  return mapGet(map, STRING_VAL(key), value);
}

static void adjustMapCapacity(Map *map, size_t capacity) {
  size_t i;
  MapEntry *entries = ALLOCATE(MapEntry, capacity);
  MapEntry *p, *first = NULL, *last = NULL;
  for (i = 0; i < capacity; i++) {
    entries[i].key = EMPTY_KEY_VAL();
    entries[i].value = NIL_VAL();
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
  isNewKey = IS_EMPTY_KEY(entry->key);

  if (isNewKey) {
    /* If entry->value is not nil, we're reusing a tombstone
    * so we don't want to increment the count since tombstones
    * are already included in count.
    * We include tombstones in the count so that the loadfactor
    * is sensitive to slots occupied by tombstones */
    if (IS_NIL(entry->value)) {
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
  return mapSet(map, STRING_VAL(key), value);
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
  push(STRING_VAL(keystr));
  result = mapSet(map, STRING_VAL(keystr), value);
  pop(); /* keystr */
  pop(); /* value */
  return result;
}

ubool mapDelete(Map *map, Value key) {
  MapEntry *entry;

  if (map->occupied == 0) {
    return UFALSE;
  }

  entry = findMapEntry(map->entries, map->capacity, key);
  if (IS_EMPTY_KEY(entry->key)) {
    return UFALSE;
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
  entry->key = EMPTY_KEY_VAL();
  entry->value = BOOL_VAL(1);
  map->size--;
  return UTRUE;
}

ubool mapDeleteStr(Map *map, String *key) {
  return mapDelete(map, STRING_VAL(key));
}

void mapAddAll(Map *from, Map *to) {
  size_t i;
  for (i = 0; i < from->capacity; i++) {
    MapEntry *entry = &from->entries[i];
    if (!IS_EMPTY_KEY(entry->key)) {
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
    if (IS_EMPTY_KEY(entry->key)) {
      /* Stop if we find an empty non-tombstone entry */
      if (IS_NIL(entry->value)) {
        return NULL;
      }
    } else if (IS_FROZEN_LIST(entry->key)) {
      ObjFrozenList *key = AS_FROZEN_LIST(entry->key);
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
    if (IS_EMPTY_KEY(entry->key)) {
      /* Stop if we find an empty non-tombstone entry */
      if (IS_NIL(entry->value)) {
        return NULL;
      }
    } else if (IS_FROZEN_DICT(entry->key)) {
      ObjFrozenDict *key = AS_FROZEN_DICT(entry->key);
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
    if (!IS_EMPTY_KEY(entry->key) &&
        IS_OBJ(entry->key) &&
        !AS_OBJ(entry->key)->isMarked) {
      mapDelete(map, entry->key);
    }
  }
}

void markMap(Map *map) {
  size_t i;
  for (i = 0; i < map->capacity; i++) {
    MapEntry *entry = &map->entries[i];
    if (!IS_EMPTY_KEY(entry->key)) {
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
    return UTRUE;
  }
  return UFALSE;
}

ubool mapIteratorNextKey(MapIterator *di, Value *out) {
  if (di->entry) {
    *out = di->entry->key;
    di->entry = di->entry->next;
    return UTRUE;
  }
  return UFALSE;
}

#include "mtots2map.h"

#include <stdlib.h>

#include "mtots1err.h"
#include "mtots2structs.h"

#define MAP_MAX_LOAD 0.75

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

#define IS_SENTINEL(v) ((v).type == VALUE_SENTINEL)
#define IS_NIL(v) ((v).type == VALUE_NIL)

void retainMap(Map *map) {
  retainObject((Object *)map);
}

void releaseMap(Map *map) {
  releaseObject((Object *)map);
}

Value mapValue(Map *map) {
  return objectValue((Object *)map);
}

ubool isMap(Value value) {
  return isObject(value) && value.as.object->type == OBJECT_MAP;
}

Map *asMap(Value value) {
  if (!isMap(value)) {
    panic("Expected Map but got %s", getValueKindName(value));
  }
  return (Map *)value.as.object;
}

void reprMap(String *out, Map *map) {
  MapEntry *entry;
  msputc('{', out);
  for (entry = map->first; entry; entry = entry->next) {
    if (entry != map->first) {
      msputs(", ", out);
    }
    reprValue(out, entry->key);
    msputs(": ", out);
    reprValue(out, entry->value);
  }
  msputc('}', out);
}

ubool eqMap(Map *a, Map *b) {
  MapEntry *entry;
  if (a == b) {
    return UTRUE;
  }
  if (a->size != b->size) {
    return UFALSE;
  }
  for (entry = a->first; entry; entry = entry->next) {
    if (!eqValue(entry->value, mapGet(b, entry->key))) {
      return UFALSE;
    }
  }
  return UTRUE;
}

u32 hashMap(Map *map) {
  return map->hash;
}

void freezeMap(Map *map) {
  if (map->frozen) {
    return;
  }
  map->frozen = UTRUE;

  /* TODO: better hash */
  map->hash = (u32)map->size;
}

size_t lenMap(Map *map) {
  return map->size;
}

NODISCARD Map *newMap(void) {
  Map *map = (Map *)calloc(1, sizeof(Map));
  map->object.type = OBJECT_MAP;
  return map;
}

/* NOTE: capacity should always be non-zero.
 * If findEntry was a non-static function, I probably would do the check
 * in the function itself. But since it is static, all places where
 * it can be called are within this file and should be manually checked.
 */
static MapEntry *findEntry(MapEntry *entries, size_t capacity, Value key) {
  /* OPT: key->hash % capacity */
  u32 index = hashValue(key) & (capacity - 1);
  MapEntry *tombstone = NULL;
  for (;;) {
    MapEntry *entry = &entries[index];
    if (IS_SENTINEL(entry->key)) {
      if (IS_NIL(entry->value)) {
        /* Empty entry */
        return tombstone != NULL ? tombstone : entry;
      } else if (tombstone == NULL) {
        /* We found a tombstone */
        tombstone = entry;
      }
    } else if (eqValue(entry->key, key)) {
      /* We found the key */
      return entry;
    }
    /* OPT: (index + 1) % capacity */
    index = (index + 1) & (capacity - 1);
  }
}

static void adjustCapacity(Map *map, size_t capacity) {
  size_t i;
  MapEntry *entries = (MapEntry *)malloc(capacity * sizeof(MapEntry));
  MapEntry *p, *first = NULL, *last = NULL;
  for (i = 0; i < capacity; i++) {
    entries[i].key = sentinelValue();
    entries[i].value = nilValue();
  }

  map->occupied = 0;
  for (p = map->first; p; p = p->next) {
    MapEntry *dest;

    dest = findEntry(entries, capacity, p->key);
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

  free(map->entries);
  map->entries = entries;
  map->capacity = capacity;
  map->first = first;
  map->last = last;
}

/** Retrieves an entry in the Map.
 * If the given key is not found, the sentinel value is returned.
 * NOTE: Returned value is NOT retained, since the Map holds a reference
 * to the values itself */
Value mapGet(Map *map, Value key) {
  MapEntry *entry;
  if (map->size == 0) {
    return sentinelValue();
  }
  entry = findEntry(map->entries, map->capacity, key);
  return entry->key.type == VALUE_SENTINEL ? sentinelValue() : entry->value;
}

/** Overrides the mapping for the given key.
 * If this creates a new entry, returns UTRUE, otherwise returns UFALSE */
ubool mapSet(Map *map, Value key, Value value) {
  MapEntry *entry;
  ubool isNewKey;

  if (map->frozen) {
    panic("Tried to modify a frozen Map");
  }

  if (map->occupied + 1 > map->capacity * MAP_MAX_LOAD) {
    size_t capacity = GROW_CAPACITY(map->capacity);
    adjustCapacity(map, capacity);
  }
  entry = findEntry(map->entries, map->capacity, key);
  isNewKey = IS_SENTINEL(entry->key);

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
  retainValue(key);
  retainValue(value);
  if (!isNewKey) {
    releaseValue(entry->key);
    releaseValue(entry->value);
  }
  entry->key = key;
  entry->value = value;
  return isNewKey;
}

/** Delete an entry in the dictionary.
 * Returns the value associated with the key.
 * If there is no entry with the given key, returns the sentinel value.
 * NOTE: Returned value must be explicitly released */
NODISCARD Value mapPop(Map *map, Value key) {
  MapEntry *entry;
  Value ret;

  if (map->frozen) {
    panic("Tried to pop from a frozen Map");
  }

  if (map->occupied == 0) {
    return sentinelValue();
  }

  entry = findEntry(map->entries, map->capacity, key);
  if (IS_SENTINEL(entry->key)) {
    return sentinelValue();
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
  releaseValue(entry->key);
  ret = entry->value;
  entry->key = sentinelValue();
  entry->value = boolValue(UTRUE);
  map->size--;
  return ret;
}

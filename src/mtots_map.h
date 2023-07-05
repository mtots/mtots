#ifndef mtots_map_h
#define mtots_map_h

#include "mtots_value.h"

typedef struct MapEntry {
  Value key;
  Value value;
  struct MapEntry *prev;
  struct MapEntry *next;
} MapEntry;

/* TODO: Do what Python does with Map layout
 * https://mail.python.org/pipermail/python-dev/2012-December/123028.html
 */
typedef struct Map {
  /* `occupied` is stored to keep track of the load factor.
   * `occupied` is the number of entries that are either
   * contain a live entry or a tombstone.
   * To get the actual number of entries in this Map,
   * see field 'size'.
   */
  size_t occupied; /* size + tombstones */
  size_t capacity; /* 0 or (8 * <power of 2>) */
  size_t size;     /* actual number of active elements */
  MapEntry *entries;
  MapEntry *first;
  MapEntry *last;
} Map;

typedef struct MapIterator {
  MapEntry *entry;
} MapIterator;

u32 hashval(Value value);

void initMap(Map *map);
void freeMap(Map *map);
ubool mapContainsKey(Map *map, Value key);
ubool mapGet(Map *map, Value key, Value *value);
ubool mapGetStr(Map *map, String *key, Value *value);
ubool mapSet(Map *map, Value key, Value value);
ubool mapSetStr(Map *map, String *key, Value value);
ubool mapSetN(Map *map, const char *key, Value value);
ubool mapDelete(Map *map, Value key);
ubool mapDeleteStr(Map *map, String *key);
void mapAddAll(Map *from, Map *to);
struct ObjFrozenList *mapFindFrozenList(
    Map *map,
    Value *buffer,
    size_t length,
    u32 hash);
struct ObjFrozenDict *mapFindFrozenDict(
    Map *map,
    Map *frozenDictMap,
    u32 hash);
void mapRemoveWhite(Map *map);
void markMap(Map *map);

void initMapIterator(MapIterator *di, Map *map);
ubool mapIteratorNext(MapIterator *di, MapEntry **out);
ubool mapIteratorNextKey(MapIterator *di, Value *out);

#endif/*mtots_map_h*/

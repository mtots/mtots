#ifndef mtots2map_h
#define mtots2map_h

#include "mtots2object.h"

typedef struct Map Map;

extern Class MAP_CLASS;

void retainMap(Map *map);
void releaseMap(Map *map);
Value mapValue(Map *map);
ubool isMap(Value value);
Map *asMap(Value value);

void reprMap(String *out, Map *map);
ubool eqMap(Map *a, Map *b);
u32 hashMap(Map *map);
void freezeMap(Map *map);
size_t lenMap(Map *map);

NODISCARD Map *newMap(void);
Value mapGet(Map *map, Value key);
ubool mapSet(Map *map, Value key, Value value);
NODISCARD Value mapPop(Map *map, Value key);

#endif /*mtots2map_h*/

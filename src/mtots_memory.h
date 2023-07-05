#ifndef mtots_memory_h
#define mtots_memory_h

#include "mtots_value.h"

#define MAX_FOREVER_VALUE_COUNT 128

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
  (type*)reallocate(pointer, sizeof(type) * (oldCount), \
    sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

typedef struct Memory {
  size_t bytesAllocated;
  size_t nextGC;
  Obj *objects;
  size_t grayCount;
  size_t grayCapacity;
  Obj **grayStack;
  size_t foreverValueCount;
  Value foreverValues[MAX_FOREVER_VALUE_COUNT];
  size_t mallocCount;
} Memory;

void initMemory(Memory *memory);
void addForeverValue(Value value);
String *internForeverCString(const char *string);
void *reallocate(void *pointer, size_t oldSize, size_t newSize);
void markObject(Obj *object);
void markString(String *string);
void markValue(Value value);
void collectGarbage();
void freeObjects();

#endif/*mtots_memory_h*/

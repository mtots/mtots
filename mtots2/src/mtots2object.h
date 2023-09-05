#ifndef mtots2object_h
#define mtots2object_h

#include "mtots2value.h"

#define NEW_OBJECT(ctype, enumType) (ctype *)newObject(enumType, sizeof(ctype))

typedef enum ObjectType {
  OBJECT_STRING,
  OBJECT_LIST,
  OBJECT_MAP,
  OBJECT_NATIVE
} ObjectType;

struct Object {
  ObjectType type;
  u32 refcnt; /* reference count - 1 */

#if MTOTS_DEBUG_MEMORY_LEAK
  Object *next;
  Object *prev;
#endif
};

const char *getObjectTypeName(ObjectType type);

void retainObject(Object *object);
void releaseObject(Object *object);

Object *newObject(ObjectType type, size_t size);

Class *getClassOfObject(Object *object);
ubool testObject(Object *a);
void reprObject(String *out, Object *object);
ubool eqObject(Object *a, Object *b);
u32 hashObject(Object *a);

#if MTOTS_DEBUG_MEMORY_LEAK
void printLeakedObjects(void);
#endif

#endif /*mtots2object_h*/

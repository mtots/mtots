#ifndef mtots2object_h
#define mtots2object_h

#include "mtots2value.h"

typedef enum ObjectType {
  OBJECT_STRING,
  OBJECT_LIST,
  OBJECT_MAP,
  OBJECT_NATIVE
} ObjectType;

struct Object {
  ObjectType type;
  u32 refcnt; /* reference count - 1 */
};

const char *getObjectTypeName(ObjectType type);

void retainObject(Object *object);
void releaseObject(Object *object);

Class *getClassOfObject(Object *object);
void reprObject(String *out, Object *object);
ubool eqObject(Object *a, Object *b);
u32 hashObject(Object *a);

#endif /*mtots2object_h*/

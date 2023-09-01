#include "mtots2object.h"

#include <stdlib.h>

#include "mtots1err.h"
#include "mtots2structs.h"

const char *getObjectTypeName(ObjectType type) {
  switch (type) {
    case OBJECT_STRING:
      return "String";
  }
  return "INVALID_OBJECT_TYPE";
}

static void freeObject(Object *object) {
  switch (object->type) {
    case OBJECT_STRING: {
      String *string = (String *)object;
      free(string->utf8);
      free(string->utf32);
      break;
    }
  }
  free(object);
}

void retainObject(Object *object) {
  object->refcnt++;
}

void releaseObject(Object *object) {
  if (object->refcnt) {
    object->refcnt--;
  } else {
    freeObject(object);
  }
}

void reprObject(String *out, Object *object) {
  switch (object->type) {
    case OBJECT_STRING:
      reprString(out, (String *)object);
      return;
  }
  panic("INVALID OBJECT TYPE %s/%d (reprObject)",
        getObjectTypeName(object->type),
        object->type);
}

ubool eqObject(Object *a, Object *b) {
  if (a == b) {
    return UTRUE;
  }
  if (a->type != b->type) {
    return UFALSE;
  }
  switch (a->type) {
    case OBJECT_STRING:
      return eqString((String *)a, (String *)b);
  }
  panic("INVALID OBJECT TYPE %s/%d (eqObject)",
        getObjectTypeName(a->type),
        a->type);
}

u32 hashObject(Object *a) {
  switch (a->type) {
    case OBJECT_STRING:
      return hashString((String *)a);
  }
  panic("INVALID OBJECT TYPE %s/%d (hashObject)",
        getObjectTypeName(a->type),
        a->type);
}

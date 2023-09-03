#include "mtots2object.h"

#include <stdlib.h>

#include "mtots1err.h"
#include "mtots2native.h"
#include "mtots2structs.h"

const char *getObjectTypeName(ObjectType type) {
  switch (type) {
    case OBJECT_STRING:
      return "String";
    case OBJECT_LIST:
      return "List";
    case OBJECT_MAP:
      return "Map";
    case OBJECT_NATIVE:
      return "Native";
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
    case OBJECT_LIST: {
      List *list = (List *)object;
      free(list->buffer);
      break;
    }
    case OBJECT_MAP: {
      Map *map = (Map *)object;
      free(map->entries);
      break;
    }
    case OBJECT_NATIVE: {
      Native *n = (Native *)object;
      n->descriptor->destructor(n);
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
    case OBJECT_LIST:
      reprList(out, (List *)object);
      return;
    case OBJECT_MAP:
      reprMap(out, (Map *)object);
      return;
    case OBJECT_NATIVE:
      reprNative(out, (Native *)object);
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
    case OBJECT_LIST:
      return eqList((List *)a, (List *)b);
    case OBJECT_MAP:
      return eqMap((Map *)a, (Map *)b);
    case OBJECT_NATIVE:
      return eqNative((Native *)a, (Native *)b);
  }
  panic("INVALID OBJECT TYPE %s/%d (eqObject)",
        getObjectTypeName(a->type),
        a->type);
}

u32 hashObject(Object *a) {
  switch (a->type) {
    case OBJECT_STRING:
      return hashString((String *)a);
    case OBJECT_LIST:
      return hashList((List *)a);
    case OBJECT_MAP:
      return hashMap((Map *)a);
    case OBJECT_NATIVE:
      return hashNative((Native *)a);
  }
  panic("INVALID OBJECT TYPE %s/%d (hashObject)",
        getObjectTypeName(a->type),
        a->type);
}

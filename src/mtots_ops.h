#ifndef mtots_ops_h
#define mtots_ops_h

#include "mtots_object.h"

ubool valuesIs(Value a, Value b);
ubool mapsEqual(Map *a, Map *b);
ubool valuesEqual(Value a, Value b);
ubool valueLessThan(Value a, Value b);
void listAppend(ObjList *list, Value value);
void sortList(ObjList *list, ObjList *keys);
Status sortListWithKeyFunc(ObjList *list, Value keyfunc);
Status valueRepr(StringBuilder *out, Value value);
Status valueStr(StringBuilder *out, Value value);
Status strMod(StringBuilder *out, const char *format, ObjList *args);
Status valueLen(Value recv, size_t *out);
Status valueIter(Value iterable, Value *out);
Status valueIterNext(Value iterator, Value *out);
Status valueFastIter(Value iterable, Value *out);
Status valueFastIterNext(Value *iterator, Value *out);
Status valueGetItem(Value owner, Value key, Value *out);
Status valueSetItem(Value owner, Value key, Value value);

#endif /*mtots_ops_h*/

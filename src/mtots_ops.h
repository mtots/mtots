#ifndef mtots_ops_h
#define mtots_ops_h

#include "mtots_object.h"

ubool valuesIs(Value a, Value b);
ubool mapsEqual(Map *a, Map *b);
ubool valuesEqual(Value a, Value b);
ubool valueLessThan(Value a, Value b);
void sortList(ObjList *list, ObjList *keys);
ubool sortListWithKeyFunc(ObjList *list, Value keyfunc);
ubool valueRepr(StringBuilder *out, Value value);
ubool valueStr(StringBuilder *out, Value value);
ubool strMod(StringBuilder *out, const char *format, ObjList *args);
ubool valueLen(Value recv, size_t *out);
Status valueIter(Value iterable, Value *out);
Status valueIterNext(Value iterator, Value *out);
Status valueFastIter(Value iterable, Value *out);
Status valueFastIterNext(Value *iterator, Value *out);
ubool valueGetItem(Value owner, Value key, Value *out);
ubool valueSetItem(Value owner, Value key, Value value);

#endif /*mtots_ops_h*/

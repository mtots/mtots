#ifndef mtots_ops_h
#define mtots_ops_h

#include "mtots_object.h"

ubool valuesIs(Value a, Value b);
ubool mapsEqual(Map *a, Map *b);
ubool valuesEqual(Value a, Value b);
ubool valueLessThan(Value a, Value b);
void sortList(ObjList *list, ObjList *keys);
ubool sortListWithKeyFunc(ObjList *list, Value keyfunc);
ubool valueRepr(Buffer *out, Value value);
ubool valueStr(Buffer *out, Value value);
ubool strMod(Buffer *out, const char *format, ObjList *args);
ubool valueLen(Value recv, size_t *out);
ubool valueIter(Value iterable, Value *out);
ubool valueIterNext(Value iterator, Value *out);
ubool valueFastIter(Value iterable, Value *out);
ubool valueFastIterNext(Value *iterator, Value *out);
ubool valueGetItem(Value owner, Value key, Value *out);
ubool valueSetItem(Value owner, Value key, Value value);

#endif/*mtots_ops_h*/

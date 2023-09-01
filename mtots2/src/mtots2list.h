#ifndef mtots2list_h
#define mtots2list_h

#include "mtots2object.h"

typedef struct List List;

void retainList(List *list);
void releaseList(List *list);
Value listValue(List *list);
ubool isList(Value value);
List *asList(Value value);

void reprList(String *out, List *list);
ubool eqList(List *a, List *b);
size_t lenList(List *list);

NODISCARD List *newList(size_t length);
void listAppend(List *list, Value value);
NODISCARD Value listPop(List *list);
Value listGet(List *list, size_t index);
void listSet(List *list, size_t index, Value value);

#endif /*mtots2list_h*/

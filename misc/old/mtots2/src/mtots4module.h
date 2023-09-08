#ifndef mtots4module_h
#define mtots4module_h

#include "mtots2object.h"

typedef struct Module Module;

void retainModule(Module *module);
void releaseModule(Module *module);
Value moduleValue(Module *module);
ubool isModule(Value value);
Module *asModule(Value value);

Module *newModule(Symbol *moduleName, Map *scope);

Map *moduleScope(Module *module);

Class *getModuleClass(void);

#endif /*mtots4module_h*/

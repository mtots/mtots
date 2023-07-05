#ifndef mtots_import_h
#define mtots_import_h

#include "mtots_object.h"

ubool importModuleWithPathAndSource(
    String *moduleName, const char *path, const char *source,
    ubool freePath, ubool freeSource);
ubool importModuleWithPath(String *moduleName, const char *path);
ubool importModule(String *moduleName);
ubool importModuleAndPop(const char *moduleName);

#endif/*mtots_import_h*/

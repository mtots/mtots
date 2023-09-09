#ifndef mtots_import_h
#define mtots_import_h

#include "mtots_object.h"

Status importModuleWithPathAndSource(
    String *moduleName, const char *path, const char *source,
    char *freePath, char *freeSource);
Status importModuleWithPath(String *moduleName, const char *path);
Status importModule(String *moduleName);
Status importModuleAndPop(const char *moduleName);

#endif /*mtots_import_h*/

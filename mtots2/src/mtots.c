#include "mtots.h"

#include <stdlib.h>

#include "mtots1err.h"
#include "mtots3parser.h"
#include "mtots4eval.h"
#include "mtots4globals.h"
#include "mtots4readfile.h"

typedef struct ModuleData ModuleData;

struct ModuleData {
  Symbol *moduleName; /* name of the module (for importing) */
  Symbol *filePath;   /* path to the file, if any */
  Ast *ast;           /* parsed module AST */
  Map *scope;         /* scope of the Module */
  ModuleData *next;   /* next ModuleData in the chain of all ModuleData */
};

struct Mtots {
  Map *globals;
  ModuleData *moduleDataList;
};

#if MTOTS_DEBUG_MEMORY_LEAK
/* Will check for memory leaks if MTOTS_DEBUG_MEMORY_LEAK is true.
 * Otherwise, this function will does nothing */
static void checkForMemoryLeaks(void) {
  freeAllClassData();
  {
    size_t objectCount = printLeakedObjects();
    size_t astCount = printLeakedAsts();
    if (objectCount || astCount) {
      panic("MEMORY LEAK DETECTED (objects = %lu, ast-nodes = %lu)",
            (unsigned long)objectCount,
            (unsigned long)astCount);
    }
  }
}
#endif

Mtots *newMtots(void) {
  Mtots *mtots = (Mtots *)calloc(1, sizeof(Mtots));
  initClassData();
  mtots->globals = newGlobals();
  return mtots;
}

void releaseMtots(Mtots *mtots) {
  ModuleData *data = mtots->moduleDataList;
  while (data) {
    ModuleData *next = data->next;
    freeAst(data->ast);
    mapClear(data->scope);
    releaseMap(data->scope);
    free(data);
    data = next;
  }
  releaseMap(mtots->globals);
  free(mtots);
#if MTOTS_DEBUG_MEMORY_LEAK
  checkForMemoryLeaks();
#endif
}

static ModuleData *newModuleData(Mtots *mtots, Symbol *moduleName) {
  ModuleData *data = (ModuleData *)calloc(1, sizeof(ModuleData));
  data->moduleName = moduleName;
  data->next = mtots->moduleDataList;
  mtots->moduleDataList = data;
  return data;
}

Status runMtotsFile(Mtots *mtots, const char *filePath) {
  char *source = readFileAsString(filePath);
  ModuleData *data = newModuleData(mtots, NULL);
  data->filePath = newSymbol(filePath);
  data->scope = newMapWithParent(mtots->globals);
  if (!parse(source, &data->ast)) {
    free(source);
    return STATUS_ERR;
  }
  free(source);
  if (!evalAst(data->ast, data->scope)) {
    freeAst(data->ast);
    return STATUS_ERR;
  }
  return STATUS_OK;
}

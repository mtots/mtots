#include <stdio.h>
#include <stdlib.h>

#include "mtots1err.h"
#include "mtots3parser.h"
#include "mtots4eval.h"
#include "mtots4globals.h"
#include "mtots4readfile.h"

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    panic("Usage mtots <file-path>");
  }
  {
    const char *path = argv[1];
    Ast *ast;
    char *source = readFileAsString(path);
    Map *globals = newGlobals();
    Map *scope = newMapWithParent(globals);
    releaseMap(globals);
    if (!parse(source, &ast)) {
      free(source);
      panic("%s", getErrorString());
    }
    free(source);
    if (!evalAst(ast, scope)) {
      freeAst(ast);
      panic("%s", getErrorString());
    }
    freeAst(ast);
    mapClear(scope);
    releaseMap(scope);
  }
#if MTOTS_DEBUG_MEMORY_LEAK
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
#endif
  return 0;
}

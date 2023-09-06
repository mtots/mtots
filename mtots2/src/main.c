#include <stdio.h>
#include <stdlib.h>

#include "mtots1err.h"
#include "mtots3parser.h"
#include "mtots4eval.h"
#include "mtots4readfile.h"

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    panic("Usage mtots <file-path>");
  }
  {
    const char *path = argv[1];
    Ast *ast;
    char *source = readFileAsString(path);
    if (!parse(source, &ast)) {
      free(source);
      panic("%s", getErrorString());
    }
    free(source);
    if (!evalAst(ast)) {
      freeAst(ast);
      panic("%s", getErrorString());
    }
    freeAst(ast);
    freeGlobals();
  }
#if MTOTS_DEBUG_MEMORY_LEAK
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

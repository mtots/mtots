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
      panic("%s", getErrorString());
    }
  }
  return 0;
}

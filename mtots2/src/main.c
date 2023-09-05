#include <stdio.h>

#include "mtots1err.h"
#include "mtots3parser.h"
#include "mtots4eval.h"

int main() {
  Ast *ast;
  const char *source = "print(repr('Hello world!' + \"extra\"))";
  if (!parse(source, &ast)) {
    panic("%s", getErrorString());
  }
  if (!evalAst(ast)) {
    panic("%s", getErrorString());
  }
  return 0;
}

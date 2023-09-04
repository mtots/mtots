#include <stdio.h>

#include "mtots1err.h"
#include "mtots3parser.h"
#include "mtots4eval.h"

int main() {
  Ast *ast;
  const char *source = "print(repr('Hello world!'))";
  if (!parse(source, &ast)) {
    panic("%s", getErrorString());
  }
  switch (evalAst(ast)) {
    case EVAL_STATUS_OK:
      break;
    case EVAL_STATUS_ERR:
      panic("%s", getErrorString());
    case EVAL_STATUS_RETURN:
      panic("Return from AST outside of fucntion");
  }
  return 0;
}

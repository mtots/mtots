#include <stdio.h>

#include "mtots1err.h"
#include "mtots3parser.h"
#include "mtots4eval.h"

static Status implPrint(i16 argc, Value *argv, Value *out) {
  printValue(argv[0]);
  printf("\n");
  return STATUS_OK;
}

static CFunction funcPrint = {"print", 1, 0, implPrint};

static void initGlobals() {
  setEvalGlobal(newSymbol("print"), cfunctionValue(&funcPrint));
}

int main() {
  Ast *ast;
  const char *source = "print('Hello world!')";
  initGlobals();
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

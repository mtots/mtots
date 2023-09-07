#include "mtots_repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

/* State stored for the repl in case something goes wrong and
 * we need to recover
 *
 * TODO: Figure out a cleaner way to handle this stuff. And check
 * whether I should be worrying about the try-stack or vm.frames,
 * or anything else.
 */
static Value *replStackTop;
static i16 replFrameCount;

static void saveState(void) {
  replStackTop = vm.stackTop;
  replFrameCount = vm.frameCount;
}

static void restoreState(void) {
  closeUpvalues(replStackTop);
  vm.stackTop = replStackTop;
  vm.frameCount = replFrameCount;
}

void repl(void) {
  char line[1024];
  ObjModule *module;
  String *mainModuleName;
  mainModuleName = internCString("__main__");
  push(STRING_VAL(mainModuleName));
  module = newModule(mainModuleName, UTRUE);
  push(MODULE_VAL(module));
  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    saveState();
    if (interpret(line, module)) {
      pop(); /* return value */
    } else {
      fprintf(stderr, "%s", getErrorString());
      restoreState();
    }
  }
  pop(); /* module */
  pop(); /* mainModuleName */
}

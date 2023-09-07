#include <stdio.h>
#include <stdlib.h>

#include "mtots.h"

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    panic("Usage mtots <file-path>");
  }
  {
    Mtots *mtots = newMtots();
    const char *path = argv[1];
    if (!runMtotsFile(mtots, path)) {
      panic("%s", getErrorString());
    }
    releaseMtots(mtots);
  }
  return 0;
}

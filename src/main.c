#include "mtots_main.h"

#if defined(__IPHONEOS__) || defined(__TVOS__)
#include "SDL.h"
#endif

int main(int argc, const char *argv[]) {
  return mtotsMain(argc, argv);
}

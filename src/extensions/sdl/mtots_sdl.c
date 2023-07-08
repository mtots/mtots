#include "mtots_sdl.h"

#include "mtots_vm.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>


ubool sdlError(const char *functionName) {
  runtimeError("%s: %s", functionName, SDL_GetError());
  return UFALSE;
}

ubool initSDL(void) {
  if (SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER) != 0) {
    return sdlError("SDL_Init");
  }
  return UTRUE;
}

#ifndef mtots_sdl_h
#define mtots_sdl_h

/*
 * When SDL is available
 * the functions here are meant to fascilitate
 * coordination between the multiple modules that
 * might need SDL.
 */

#include "mtots_common.h"

ubool sdlError(const char *functionName);
ubool initSDL(void);

#endif/*mtots_sdl_h*/

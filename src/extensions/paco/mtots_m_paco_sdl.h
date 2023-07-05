#ifndef mtots_m_paco_sdl_h
#define mtots_m_paco_sdl_h

#include "mtots_m_paco_common.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#ifdef __EMSCRIPTEN__
typedef int Mutex;
Mutex *CREATE_MUTEX() { static Mutex m; return &m; }
int LOCK_MUTEX  (Mutex *m) { return 0; }
int UNLOCK_MUTEX(Mutex *m) { return 0; }
#else
typedef SDL_mutex Mutex;
#define CREATE_MUTEX  SDL_CreateMutex
#define LOCK_MUTEX    SDL_LockMutex
#define UNLOCK_MUTEX  SDL_UnlockMutex
#endif

#define BUTTON_LEFT    0
#define BUTTON_RIGHT   1
#define BUTTON_UP      2
#define BUTTON_DOWN    3
#define BUTTON_O       4
#define BUTTON_X       5


static SDL_Color systemColors[] = {
  /* PICO-8's official 16 colors */
  {   0,   0,   0, 255 }, /* black */
  {  29,  43,  83, 255 }, /* dark-blue */
  { 126,  37,  83, 255 }, /* dark-purple */
  {   0, 135,  81, 255 }, /* dark-green */
  { 171,  82,  54, 255 }, /* brown */
  {  95,  87,  79, 255 }, /* dark-grey */
  { 194, 195, 199, 255 }, /* light-grey */
  { 255, 241, 232, 255 }, /* white */
  { 255,   0,  77, 255 }, /* red */
  { 255, 163,   0, 255 }, /* orange */
  { 255, 236,  39, 255 }, /* yellow */
  {   0, 228,  54, 255 }, /* green */
  {  41, 173, 255, 255 }, /* blue */
  { 131, 118, 156, 255 }, /* lavender */
  { 255, 119, 168, 255 }, /* pink */
  { 255, 204, 170, 255 }, /* light-peach */

  /* PICO-8's unofficial 16 colors */
  {  41,  24,  20, 255 }, /* brownish-black */
  {  17,  29,  53, 255 }, /* darker-blue */
  {  66,  33,  54, 255 }, /* darker-purple */
  {  18,  83,  89, 255 }, /* blue-green */
  { 116,  47,  41, 255 }, /* dark-brown */
  {  73,  51,  59, 255 }, /* darker-grey */
  { 162, 136, 121, 255 }, /* medium-grey */
  { 243, 239, 125, 255 }, /* light-yellow */
  { 190,  18,  80, 255 }, /* dark-red */
  { 255, 108,  36, 255 }, /* dark-orange */
  { 168, 231,  46, 255 }, /* lime-green */
  {   0, 181,  67, 255 }, /* medium-green */
  {   6,  90, 181, 255 }, /* true-blue */
  { 117,  70, 101, 255 }, /* mauve */
  { 255, 110,  89, 255 }, /* dark-peach */
  { 255, 157, 129, 255 }, /* peach */
};

/* Taken from
 * https://github.com/spurious/SDL-mirror/blob/master/src/video/SDL_surface.c
 */
static Sint64 calculatePitch(Uint32 format, int width) {
  Sint64 pitch;
  if (SDL_ISPIXELFORMAT_FOURCC(format) || SDL_BITSPERPIXEL(format) >= 8) {
    pitch = ((Sint64)width * SDL_BYTESPERPIXEL(format));
  } else {
    pitch = (((Sint64)width * SDL_BITSPERPIXEL(format)) + 7) / 8;
  }
  pitch = (pitch + 3) & ~3;   /* 4-byte aligning for speed */
  return pitch;
}

static void addColorConstants(ObjModule *module) {
  mapSetN(&module->fields, "TRANSPARENT", NUMBER_VAL(TRANSPARENT));
  mapSetN(&module->fields, "BLACK", NUMBER_VAL(BLACK));
  mapSetN(&module->fields, "DARK_BLUE", NUMBER_VAL(DARK_BLUE));
  mapSetN(&module->fields, "DARK_PURPLE", NUMBER_VAL(DARK_PURPLE));
  mapSetN(&module->fields, "DARK_GREEN", NUMBER_VAL(DARK_GREEN));
  mapSetN(&module->fields, "BROWN", NUMBER_VAL(BROWN));
  mapSetN(&module->fields, "DARK_GREY", NUMBER_VAL(DARK_GREY));
  mapSetN(&module->fields, "LIGHT_GREY", NUMBER_VAL(LIGHT_GREY));
  mapSetN(&module->fields, "WHITE", NUMBER_VAL(WHITE));
  mapSetN(&module->fields, "RED", NUMBER_VAL(RED));
  mapSetN(&module->fields, "ORANGE", NUMBER_VAL(ORANGE));
  mapSetN(&module->fields, "YELLOW", NUMBER_VAL(YELLOW));
  mapSetN(&module->fields, "GREEN", NUMBER_VAL(GREEN));
  mapSetN(&module->fields, "BLUE", NUMBER_VAL(BLUE));
  mapSetN(&module->fields, "LAVENDER", NUMBER_VAL(LAVENDER));
  mapSetN(&module->fields, "PINK", NUMBER_VAL(PINK));
  mapSetN(&module->fields, "LIGHT_PEACH", NUMBER_VAL(LIGHT_PEACH));
  mapSetN(&module->fields, "BROWNISH_BLACK", NUMBER_VAL(BROWNISH_BLACK));
  mapSetN(&module->fields, "DARKER_BLUE", NUMBER_VAL(DARKER_BLUE));
  mapSetN(&module->fields, "DARKER_PURPLE", NUMBER_VAL(DARKER_PURPLE));
  mapSetN(&module->fields, "BLUE_GREEN", NUMBER_VAL(BLUE_GREEN));
  mapSetN(&module->fields, "DARK_BROWN", NUMBER_VAL(DARK_BROWN));
  mapSetN(&module->fields, "DARKER_GREY", NUMBER_VAL(DARKER_GREY));
  mapSetN(&module->fields, "MEDIUM_GREY", NUMBER_VAL(MEDIUM_GREY));
  mapSetN(&module->fields, "LIGHT_YELLOW", NUMBER_VAL(LIGHT_YELLOW));
  mapSetN(&module->fields, "DARK_RED", NUMBER_VAL(DARK_RED));
  mapSetN(&module->fields, "DARK_ORANGE", NUMBER_VAL(DARK_ORANGE));
  mapSetN(&module->fields, "LIME_GREEN", NUMBER_VAL(LIME_GREEN));
  mapSetN(&module->fields, "MEDIUM_GREEN", NUMBER_VAL(MEDIUM_GREEN));
  mapSetN(&module->fields, "TRUE_BLUE", NUMBER_VAL(TRUE_BLUE));
  mapSetN(&module->fields, "MAUVE", NUMBER_VAL(MAUVE));
  mapSetN(&module->fields, "DARK_PEACH", NUMBER_VAL(DARK_PEACH));
  mapSetN(&module->fields, "PEACH", NUMBER_VAL(PEACH));
}

#endif/*mtots_m_paco_sdl_h*/

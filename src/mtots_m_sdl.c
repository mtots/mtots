#include "mtots_m_sdl.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_ENABLE_SDL
#include <SDL2/SDL.h>

#include "mtots_macros.h"

#define WRAP_SDL_FUNCTION(name, minArgc, maxArgc, expression)   \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    if ((expression) != 0) {                                    \
      sdlError("SDL_" #name);                                   \
      return STATUS_ERROR;                                      \
    }                                                           \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, minArgc, maxArgc};

WRAP_C_TYPE(Point, SDL_Point)
DEFINE_METHOD_COPY(Point)
DEFINE_FIELD_GETTER(Point, x, valNumber(owner->handle.x))
DEFINE_FIELD_GETTER(Point, y, valNumber(owner->handle.y))
DEFINE_FIELD_SETTER(Point, x, owner->handle.x = asInt(value))
DEFINE_FIELD_SETTER(Point, y, owner->handle.y = asInt(value))
static CFunction *PointMethods[] = {
    &funcPoint_copy,
    &funcPoint_getx,
    &funcPoint_gety,
    &funcPoint_setx,
    &funcPoint_sety,
    NULL,
};

WRAP_C_TYPE(FPoint, SDL_FPoint)
DEFINE_METHOD_COPY(FPoint)
DEFINE_FIELD_GETTER(FPoint, x, valNumber(owner->handle.x))
DEFINE_FIELD_GETTER(FPoint, y, valNumber(owner->handle.y))
DEFINE_FIELD_SETTER(FPoint, x, owner->handle.x = asFloat(value))
DEFINE_FIELD_SETTER(FPoint, y, owner->handle.y = asFloat(value))
static CFunction *FPointMethods[] = {
    &funcFPoint_copy,
    &funcFPoint_getx,
    &funcFPoint_gety,
    &funcFPoint_setx,
    &funcFPoint_sety,
    NULL,
};

WRAP_C_TYPE(Rect, SDL_Rect)
DEFINE_METHOD_COPY(Rect)
DEFINE_FIELD_GETTER(Rect, x, valNumber(owner->handle.x))
DEFINE_FIELD_GETTER(Rect, y, valNumber(owner->handle.y))
DEFINE_FIELD_GETTER(Rect, w, valNumber(owner->handle.w))
DEFINE_FIELD_GETTER(Rect, h, valNumber(owner->handle.h))
DEFINE_FIELD_SETTER(Rect, x, owner->handle.x = asInt(value))
DEFINE_FIELD_SETTER(Rect, y, owner->handle.y = asInt(value))
DEFINE_FIELD_SETTER(Rect, w, owner->handle.w = asInt(value))
DEFINE_FIELD_SETTER(Rect, h, owner->handle.h = asInt(value))
static CFunction *RectMethods[] = {
    &funcRect_copy,
    &funcRect_getx,
    &funcRect_gety,
    &funcRect_getw,
    &funcRect_geth,
    &funcRect_setx,
    &funcRect_sety,
    &funcRect_setw,
    &funcRect_seth,
    NULL,
};

WRAP_C_TYPE(FRect, SDL_FRect)
DEFINE_METHOD_COPY(FRect)
DEFINE_FIELD_GETTER(FRect, x, valNumber(owner->handle.x))
DEFINE_FIELD_GETTER(FRect, y, valNumber(owner->handle.y))
DEFINE_FIELD_GETTER(FRect, w, valNumber(owner->handle.w))
DEFINE_FIELD_GETTER(FRect, h, valNumber(owner->handle.h))
DEFINE_FIELD_SETTER(FRect, x, owner->handle.x = asFloat(value))
DEFINE_FIELD_SETTER(FRect, y, owner->handle.y = asFloat(value))
DEFINE_FIELD_SETTER(FRect, w, owner->handle.w = asFloat(value))
DEFINE_FIELD_SETTER(FRect, h, owner->handle.h = asFloat(value))
static CFunction *FRectMethods[] = {
    &funcFRect_copy,
    &funcFRect_getx,
    &funcFRect_gety,
    &funcFRect_getw,
    &funcFRect_geth,
    &funcFRect_setx,
    &funcFRect_sety,
    &funcFRect_setw,
    &funcFRect_seth,
    NULL,
};

WRAP_C_TYPE(Color, SDL_Color)
DEFINE_METHOD_COPY(Color)
DEFINE_FIELD_GETTER(Color, r, valNumber(owner->handle.r))
DEFINE_FIELD_GETTER(Color, g, valNumber(owner->handle.g))
DEFINE_FIELD_GETTER(Color, b, valNumber(owner->handle.b))
DEFINE_FIELD_GETTER(Color, a, valNumber(owner->handle.a))
DEFINE_FIELD_SETTER(Color, r, owner->handle.r = asFloat(value))
DEFINE_FIELD_SETTER(Color, g, owner->handle.g = asFloat(value))
DEFINE_FIELD_SETTER(Color, b, owner->handle.b = asFloat(value))
DEFINE_FIELD_SETTER(Color, a, owner->handle.a = asFloat(value))
static CFunction *ColorMethods[] = {
    &funcColor_copy,
    &funcColor_getr,
    &funcColor_getg,
    &funcColor_getb,
    &funcColor_geta,
    &funcColor_setr,
    &funcColor_setg,
    &funcColor_setb,
    &funcColor_seta,
    NULL,
};

WRAP_C_TYPE(Event, SDL_Event)
DEFINE_METHOD_COPY(Event)
DEFINE_FIELD_GETTER(Event, type, valNumber(owner->handle.type))
static CFunction *EventMethods[] = {
    &funcEvent_copy,
    &funcEvent_gettype,
    NULL,
};

WRAP_C_TYPE(Surface, SDL_Surface *)
DEFINE_FIELD_GETTER(Surface, w, valNumber(owner->handle->w))
DEFINE_FIELD_GETTER(Surface, h, valNumber(owner->handle->h))
static CFunction *SurfaceMethods[] = {
    &funcSurface_getw,
    &funcSurface_geth,
    NULL,
};

WRAP_C_TYPE(Texture, SDL_Texture *)
static CFunction *TextureMethods[] = {NULL};

WRAP_C_TYPE(Window, SDL_Window *)
static CFunction *WindowMethods[] = {NULL};

WRAP_C_TYPE(Renderer, SDL_Renderer *)
static CFunction *RendererMethods[] = {NULL};

static Status sdlError(const char *functionName) {
  runtimeError("%s: %s", functionName, SDL_GetError());
  return STATUS_ERROR;
}

WRAP_SDL_FUNCTION(Init, 1, 0, SDL_Init(asU32(argv[0])))
WRAP_C_FUNCTION(Quit, 0, 0, SDL_Quit())
WRAP_C_FUNCTION(
    PollEvent, 1, 0,
    *out = valBool(SDL_PollEvent(isNil(argv[0]) ? NULL : &asEvent(argv[0])->handle)))
WRAP_C_FUNCTION(Delay, 1, 0, SDL_Delay(asU32(argv[0])))
WRAP_C_FUNCTION(GetPerformanceCounter, 0, 0, *out = valNumber(SDL_GetPerformanceCounter()))
WRAP_C_FUNCTION(GetPerformanceFrequency, 0, 0, *out = valNumber(SDL_GetPerformanceFrequency()))
WRAP_SDL_FUNCTION(
    CreateWindowAndRenderer, 5, 0,
    SDL_CreateWindowAndRenderer(
        asInt(argv[0]) /* width */,
        asInt(argv[1]) /* height */,
        asU32Bits(argv[2]) /* window_flags */,
        &asWindow(argv[3])->handle,
        &asRenderer(argv[4])->handle))
WRAP_SDL_FUNCTION(
    SetRenderDrawColor, 5, 0,
    SDL_SetRenderDrawColor(
        asRenderer(argv[0])->handle,
        asU8(argv[1]) /* r */,
        asU8(argv[2]) /* g */,
        asU8(argv[3]) /* b */,
        asU8(argv[4]) /* a */))
WRAP_SDL_FUNCTION(
    RenderClear, 1, 0,
    SDL_RenderClear(asRenderer(argv[0])->handle))
WRAP_SDL_FUNCTION(
    RenderFillRect, 2, 0,
    SDL_RenderFillRect(
        asRenderer(argv[0])->handle,
        &asRect(argv[1])->handle))
WRAP_SDL_FUNCTION(
    RenderCopy, 4, 0,
    SDL_RenderCopy(
        asRenderer(argv[0])->handle,
        asTexture(argv[1])->handle,
        isNil(argv[2]) ? NULL : &asRect(argv[2])->handle /* srcrect */,
        isNil(argv[3]) ? NULL : &asRect(argv[3])->handle /* dstrect */))
WRAP_C_FUNCTION(RenderPresent, 1, 0, SDL_RenderPresent(asRenderer(argv[0])->handle))
WRAP_C_FUNCTION(
    RenderGetViewport, 2, 0,
    SDL_RenderGetViewport(
        asRenderer(argv[0])->handle,
        &asRect(argv[1])->handle))

/* ==================================================
 * Hand-implemented functions / ad-hoc functions
 * ================================================== */

static Status implGetMouseState(i16 argc, Value *argv, Value *out) {
  ObjPoint *point = asPoint(argv[0]);
  int x, y;
  *out = valNumber(SDL_GetMouseState(&x, &y));
  point->handle.x = x;
  point->handle.y = y;
  return STATUS_OK;
}

static CFunction funcGetMouseState = {
    implGetMouseState,
    "GetMouseState",
    1,
};

static Status implCreateTextureFromSurface(i16 argc, Value *argv, Value *out) {
  SDL_Renderer *renderer = asRenderer(argv[0])->handle;
  SDL_Surface *surface = asSurface(argv[1])->handle;
  SDL_Texture *handle = SDL_CreateTextureFromSurface(renderer, surface);
  ObjTexture *texture;
  if (!handle) {
    return sdlError("SDL_CreateTextureFromSurface");
  }
  texture = allocTexture();
  texture->handle = handle;
  *out = valTexture(texture);
  return STATUS_OK;
}

static CFunction funcCreateTextureFromSurface = {
    implCreateTextureFromSurface,
    "CreateTextureFromSurface",
    2,
};

/*
 * ███    ███  ██████  ██████  ██    ██ ██      ███████
 * ████  ████ ██    ██ ██   ██ ██    ██ ██      ██
 * ██ ████ ██ ██    ██ ██   ██ ██    ██ ██      █████
 * ██  ██  ██ ██    ██ ██   ██ ██    ██ ██      ██
 * ██      ██  ██████  ██████   ██████  ███████ ███████
 */

static Status impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = asModule(args[0]);
  CFunction *functions[] = {
      &funcInit,
      &funcQuit,
      &funcPollEvent,
      &funcDelay,
      &funcGetPerformanceCounter,
      &funcGetPerformanceFrequency,
      &funcCreateWindowAndRenderer,
      &funcSetRenderDrawColor,
      &funcRenderClear,
      &funcRenderFillRect,
      &funcRenderCopy,
      &funcRenderPresent,
      &funcRenderGetViewport,

      /* hand implemented functions */
      &funcGetMouseState,
      &funcCreateTextureFromSurface,
      NULL,
  };

  if (UFALSE) {
    asWindow(valNil()); /* Suppress warning */
  }

  moduleAddFunctions(module, functions);

  ADD_TYPE_TO_MODULE(Point);
  ADD_TYPE_TO_MODULE(FPoint);
  ADD_TYPE_TO_MODULE(Rect);
  ADD_TYPE_TO_MODULE(FRect);
  ADD_TYPE_TO_MODULE(Color);
  ADD_TYPE_TO_MODULE(Event);
  ADD_TYPE_TO_MODULE(Surface);
  ADD_TYPE_TO_MODULE(Texture);
  ADD_TYPE_TO_MODULE(Window);
  ADD_TYPE_TO_MODULE(Renderer);

#define ADD_INT(name) mapSetN(&module->fields, #name, valNumber(SDL_##name))
  ADD_INT(QUIT);
  ADD_INT(INIT_TIMER);
  ADD_INT(INIT_AUDIO);
  ADD_INT(INIT_VIDEO);
  ADD_INT(INIT_JOYSTICK);
  ADD_INT(INIT_HAPTIC);
  ADD_INT(INIT_GAMECONTROLLER);
  ADD_INT(INIT_EVENTS);
  ADD_INT(INIT_EVERYTHING);
#undef ADD_INT

  return STATUS_OK;
}

static CFunction func = {impl, "sdl", 1};

void addNativeModuleSDL(void) {
  addNativeModule(&func);
}

#else

static Status impl(i16 argCount, Value *args, Value *out) {
  runtimeError("No SDL support");
  return STATUS_ERROR;
}

static CFunction func = {impl, "sdl", 1};

void addNativeModuleSDL(void) {
  addNativeModule(&func);
}
#endif

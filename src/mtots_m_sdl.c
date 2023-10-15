#include "mtots_m_sdl.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_ENABLE_SDL
#include <SDL2/SDL.h>

#define WRAP_SDL_COMMON(name, ctype, freeFunc)                                    \
  typedef struct Obj##name {                                                      \
    ObjNative obj;                                                                \
    ctype handle;                                                                 \
  } Obj##name;                                                                    \
  static NativeObjectDescriptor descriptor##name = {                              \
      nopBlacken,                                                                 \
      freeFunc,                                                                   \
      sizeof(Obj##name),                                                          \
      #name,                                                                      \
  };                                                                              \
  static ubool is##name(Value value) {                                            \
    return getNativeObjectDescriptor(value) == &descriptor##name;                 \
  }                                                                               \
  static Value val##name(Obj##name *x) {                                          \
    return valObjExplicit((Obj *)x);                                              \
  }                                                                               \
  static Obj##name *as##name(Value value) {                                       \
    if (!is##name(value)) {                                                       \
      panic("Expected " #name " but got %s", getKindName(value));                 \
    }                                                                             \
    return (Obj##name *)AS_OBJ_UNSAFE(value);                                     \
  }                                                                               \
  static Obj##name *alloc##name() {                                               \
    Obj##name *ret = NEW_NATIVE(Obj##name, &descriptor##name);                    \
    memset(&ret->handle, 0, sizeof(ret->handle));                                 \
    return ret;                                                                   \
  }                                                                               \
  static Status impl##name##StaticCall(i16 argc, Value *argv, Value *out) {       \
    *out = val##name(alloc##name());                                              \
    return STATUS_OK;                                                             \
  }                                                                               \
  static CFunction func##name##StaticCall = {impl##name##StaticCall, "__call__"}; \
  static CFunction *name##StaticMethods[] = {                                     \
      &func##name##StaticCall,                                                    \
      NULL,                                                                       \
  };

#define WRAP_SDL_REF_TYPE(name, freeFunc)         \
  static void free##name(ObjNative *n);           \
  WRAP_SDL_COMMON(name, SDL_##name *, free##name) \
  static void free##name(ObjNative *n) {          \
    Obj##name *p = (Obj##name *)n;                \
    if (p->handle) {                              \
      freeFunc(p->handle);                        \
    }                                             \
  }

#define WRAP_SDL_POD_TYPE(name) \
  WRAP_SDL_COMMON(name, SDL_##name, nopFree)

#define DEFINE_METHOD_COPY(className)                                       \
  static Status impl##className##_copy(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                        \
    Obj##className *other = as##className(argv[0]);                         \
    owner->handle = other->handle;                                          \
    return STATUS_OK;                                                       \
  }                                                                         \
  static CFunction func##className##_copy = {impl##className##_copy, "copy", 2};

#define DEFINE_FIELD_GETTER(className, fieldName, getterExpression)                   \
  static Status impl##className##_get##fieldName(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                                  \
    *out = getterExpression;                                                          \
    return STATUS_OK;                                                                 \
  }                                                                                   \
  static CFunction func##className##_get##fieldName = {                               \
      impl##className##_get##fieldName,                                               \
      "__get_" #fieldName,                                                            \
  };

#define DEFINE_FIELD_SETTER(className, fieldName, setterExpression)                   \
  static Status impl##className##_set##fieldName(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                                  \
    Value value = *out = argv[0];                                                     \
    setterExpression;                                                                 \
    return STATUS_OK;                                                                 \
  }                                                                                   \
  static CFunction func##className##_set##fieldName = {                               \
      impl##className##_set##fieldName,                                               \
      "__set_" #fieldName,                                                            \
      1,                                                                              \
  };

#define ADD_TYPE_TO_MODULE(name) \
  newNativeClass(module, &descriptor##name, name##Methods, name##StaticMethods)

#define WRAP_SDL_FUNCTION_V0(name)                              \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    SDL_##name();                                               \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 0};

#define WRAP_SDL_FUNCTION_V1(name, arg0)                        \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    SDL_##name(arg0);                                           \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 1};

#define WRAP_SDL_FUNCTION_V2(name, arg0, arg1)                  \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    SDL_##name(arg0, arg1);                                     \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 2};

#define WRAP_SDL_FUNCTION_B1(name, arg0)                        \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    *out = valBool(!!SDL_##name(arg0));                         \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 1};

#define WRAP_SDL_FUNCTION_N0(name)                              \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    *out = valNumber(SDL_##name());                             \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 0};

#define WRAP_SDL_FUNCTION_I0(name)                              \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    if (SDL_##name() != 0) {                                    \
      sdlError("SDL_" #name);                                   \
      return STATUS_ERROR;                                      \
    }                                                           \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 0};

#define WRAP_SDL_FUNCTION_I1(name, arg0)                        \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    if (SDL_##name(arg0) != 0) {                                \
      sdlError("SDL_" #name);                                   \
      return STATUS_ERROR;                                      \
    }                                                           \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 1};

#define WRAP_SDL_FUNCTION_I2(name, arg0, arg1)                  \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    if (SDL_##name(arg0, arg1) != 0) {                          \
      sdlError("SDL_" #name);                                   \
      return STATUS_ERROR;                                      \
    }                                                           \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 2};

#define WRAP_SDL_FUNCTION_I3(name, arg0, arg1, arg2)            \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    if (SDL_##name(arg0, arg1, arg2) != 0) {                    \
      sdlError("SDL_" #name);                                   \
      return STATUS_ERROR;                                      \
    }                                                           \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 3};

#define WRAP_SDL_FUNCTION_I4(name, arg0, arg1, arg2, arg3)      \
  static Status impl##name(i16 argc, Value *argv, Value *out) { \
    if (SDL_##name(arg0, arg1, arg2, arg3) != 0) {              \
      sdlError("SDL_" #name);                                   \
      return STATUS_ERROR;                                      \
    }                                                           \
    return STATUS_OK;                                           \
  }                                                             \
  static CFunction func##name = {impl##name, #name, 4};

#define WRAP_SDL_FUNCTION_I5(name, arg0, arg1, arg2, arg3, arg4) \
  static Status impl##name(i16 argc, Value *argv, Value *out) {  \
    if (SDL_##name(arg0, arg1, arg2, arg3, arg4) != 0) {         \
      sdlError("SDL_" #name);                                    \
      return STATUS_ERROR;                                       \
    }                                                            \
    return STATUS_OK;                                            \
  }                                                              \
  static CFunction func##name = {impl##name, #name, 5};

WRAP_SDL_POD_TYPE(Point)
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

WRAP_SDL_POD_TYPE(FPoint)
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

WRAP_SDL_POD_TYPE(Rect)
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

WRAP_SDL_POD_TYPE(FRect)
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

WRAP_SDL_POD_TYPE(Color)
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

WRAP_SDL_POD_TYPE(Event)
DEFINE_METHOD_COPY(Event)
DEFINE_FIELD_GETTER(Event, type, valNumber(owner->handle.type))
static CFunction *EventMethods[] = {
    &funcEvent_copy,
    &funcEvent_gettype,
    NULL,
};

WRAP_SDL_REF_TYPE(Surface, SDL_FreeSurface)
DEFINE_FIELD_GETTER(Surface, w, valNumber(owner->handle->w))
DEFINE_FIELD_GETTER(Surface, h, valNumber(owner->handle->h))
static CFunction *SurfaceMethods[] = {
    &funcSurface_getw,
    &funcSurface_geth,
    NULL,
};

WRAP_SDL_REF_TYPE(Texture, SDL_DestroyTexture)
static CFunction *TextureMethods[] = {NULL};

WRAP_SDL_REF_TYPE(Window, SDL_DestroyWindow)
static CFunction *WindowMethods[] = {NULL};

WRAP_SDL_REF_TYPE(Renderer, SDL_DestroyRenderer)
static CFunction *RendererMethods[] = {NULL};

static Status sdlError(const char *functionName) {
  runtimeError("%s: %s", functionName, SDL_GetError());
  return STATUS_ERROR;
}

WRAP_SDL_FUNCTION_I1(Init, asU32(argv[0]))
WRAP_SDL_FUNCTION_V0(Quit)
WRAP_SDL_FUNCTION_B1(PollEvent, isNil(argv[0]) ? NULL : &asEvent(argv[0])->handle)
WRAP_SDL_FUNCTION_V1(Delay, asU32(argv[0]))
WRAP_SDL_FUNCTION_N0(GetPerformanceCounter)
WRAP_SDL_FUNCTION_N0(GetPerformanceFrequency)
WRAP_SDL_FUNCTION_I5(
    CreateWindowAndRenderer,
    asInt(argv[0]) /* width */,
    asInt(argv[1]) /* height */,
    asU32Bits(argv[2]) /* window_flags */,
    &asWindow(argv[3])->handle,
    &asRenderer(argv[4])->handle)
WRAP_SDL_FUNCTION_I5(
    SetRenderDrawColor,
    asRenderer(argv[0])->handle,
    asU8(argv[1]) /* r */,
    asU8(argv[2]) /* g */,
    asU8(argv[3]) /* b */,
    asU8(argv[4]) /* a */)
WRAP_SDL_FUNCTION_I1(RenderClear, asRenderer(argv[0])->handle)
WRAP_SDL_FUNCTION_I2(
    RenderFillRect,
    asRenderer(argv[0])->handle,
    &asRect(argv[1])->handle)
WRAP_SDL_FUNCTION_I4(
    RenderCopy,
    asRenderer(argv[0])->handle,
    asTexture(argv[1])->handle,
    isNil(argv[2]) ? NULL : &asRect(argv[2])->handle /* srcrect */,
    isNil(argv[3]) ? NULL : &asRect(argv[3])->handle /* dstrect */)
WRAP_SDL_FUNCTION_V1(RenderPresent, asRenderer(argv[0])->handle)
WRAP_SDL_FUNCTION_V2(
    RenderGetViewport,
    asRenderer(argv[0])->handle,
    &asRect(argv[1])->handle)

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

#include "mtots_m_sdl.h"

#include <stdlib.h>
#include <string.h>

#include "mtots.h"
#include "mtots_m_c.h"

#if MTOTS_ENABLE_SDL
#include <SDL2/SDL.h>

#if MTOTS_ENABLE_SDL_TTF
#include <SDL2_ttf/SDL_ttf.h>
#endif

#if MTOTS_ENABLE_SDL_IMAGE
#include <SDL2_image/SDL_image.h>
#endif

#if MTOTS_ENABLE_SDL_MIXER
#include <SDL2_mixer/SDL_mixer.h>
#endif

#define WRAP_CONST(name, value) \
  mapSetN(&module->fields, #name, valNumber(value))

#define WRAP_SDL_FUNCTION_EX(module, mtotsName, cname, minArgc, maxArgc, expression) \
  static Status impl##cname(i16 argc, Value *argv, Value *out) {                     \
    if ((expression) != 0) {                                                         \
      sdlError(#module "_" #mtotsName);                                              \
      return STATUS_ERROR;                                                           \
    }                                                                                \
    return STATUS_OK;                                                                \
  }                                                                                  \
  static CFunction func##cname = {impl##cname, #mtotsName, minArgc, maxArgc};

#define WRAP_SDL_FUNCTION(name, minArgc, maxArgc, expression) \
  WRAP_SDL_FUNCTION_EX(SDL, name, name, minArgc, maxArgc, expression)

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

typedef struct ObjTexture {
  ObjNative obj;
  SDL_Texture *handle;
} ObjTexture;
static void freeTexture(ObjNative *n) {
  ObjTexture *texture = (ObjTexture *)n;
  if (texture->handle) {
    SDL_DestroyTexture(texture->handle);
    texture->handle = NULL;
  }
}
WRAP_C_TYPE_EX(Texture, SDL_Texture *, static, nopBlacken, freeTexture)
static CFunction *TextureStaticMethods[] = {NULL};
static CFunction *TextureMethods[] = {NULL};

WRAP_C_TYPE(Window, SDL_Window *)
static CFunction *WindowMethods[] = {NULL};

WRAP_C_TYPE(Renderer, SDL_Renderer *)
static CFunction *RendererMethods[] = {NULL};

typedef struct ObjRWops {
  ObjNative obj;
  SDL_RWops *handle;
  Value dataOwner;
} ObjRWops;
static void blackenRWops(ObjNative *n) {
  ObjRWops *ops = (ObjRWops *)n;
  markValue(ops->dataOwner);
}
static void freeRWops(ObjNative *n) {
  ObjRWops *ops = (ObjRWops *)n;
  if (ops->handle) {
    SDL_RWclose(ops->handle);
    ops->handle = NULL;
  }
}
WRAP_C_TYPE_EX(RWops, SDL_RWops *, static, blackenRWops, freeRWops)
static CFunction *RWopsStaticMethods[] = {NULL};
static CFunction *RWopsMethods[] = {NULL};

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
WRAP_C_FUNCTION(RWFromFile, 2, 0, {
  const char *file = asString(argv[0])->chars;
  const char *mode = asString(argv[1])->chars;
  SDL_RWops *handle = SDL_RWFromFile(file, mode);
  ObjRWops *rwops;
  if (!handle) {
    return sdlError("SDL_RWFromFile");
  }
  rwops = allocRWops();
  rwops->handle = handle;
  *out = valRWops(rwops);
})
WRAP_C_FUNCTION(RWFromString, 1, 0, {
  String *string = asString(argv[0]);
  SDL_RWops *handle = SDL_RWFromConstMem(string->chars, (int)string->byteLength);
  ObjRWops *rwops;
  if (!handle) {
    return sdlError("SDL_RWFromConstMem (RWFromString)");
  }
  rwops = allocRWops();
  rwops->handle = handle;
  rwops->dataOwner = valString(string);
  *out = valRWops(rwops);
})
WRAP_C_FUNCTION(RWFromBuffer, 1, 0, {
  ObjBuffer *buffer = asBuffer(argv[0]);
  SDL_RWops *handle = SDL_RWFromMem(buffer->handle.data, (int)buffer->handle.length);
  ObjRWops *rwops;
  if (!handle) {
    return sdlError("SDL_RWFromMem (RWFromBuffer)");
  }
  bufferLock(&buffer->handle);
  rwops = allocRWops();
  rwops->handle = handle;
  rwops->dataOwner = valBuffer(buffer);
  *out = valRWops(rwops);
})
WRAP_C_FUNCTION(RWclose, 1, 0, {
  ObjRWops *rwops = asRWops(argv[0]);
  if (rwops->handle != NULL) {
    if (SDL_RWclose(rwops->handle) != 0) {
      return sdlError("SDL_RWclose");
    }
    rwops->handle = NULL;
    rwops->dataOwner = valNil();
  }
})
WRAP_C_FUNCTION(RWsize, 1, 0, *out = valNumber(SDL_RWsize(asRWops(argv[0])->handle)))
WRAP_SDL_FUNCTION(
    CreateWindowAndRenderer, 5, 0,
    SDL_CreateWindowAndRenderer(
        asInt(argv[0]) /* width */,
        asInt(argv[1]) /* height */,
        asU32Bits(argv[2]) /* window_flags */,
        &asWindow(argv[3])->handle,
        &asRenderer(argv[4])->handle))
WRAP_SDL_FUNCTION(
    SetWindowFullscreen, 2, 0,
    SDL_SetWindowFullscreen(asWindow(argv[0])->handle, asU32Bits(argv[1])))
WRAP_SDL_FUNCTION(
    SetWindowOpacity, 2, 0,
    SDL_SetWindowOpacity(asWindow(argv[0])->handle, asFloat(argv[1])))
WRAP_C_FUNCTION(
    SetWindowPosition, 3, 0,
    SDL_SetWindowPosition(asWindow(argv[0])->handle, asInt(argv[1]), asInt(argv[2])))
WRAP_C_FUNCTION(
    SetWindowResizable, 2, 0,
    SDL_SetWindowResizable(asWindow(argv[0])->handle, asBool(argv[1])))
WRAP_C_FUNCTION(
    SetWindowSize, 3, 0,
    SDL_SetWindowSize(asWindow(argv[0])->handle, asInt(argv[1]), asInt(argv[2])))
WRAP_C_FUNCTION(
    SetWindowTitle, 2, 0,
    SDL_SetWindowTitle(asWindow(argv[0])->handle, asString(argv[1])->chars))
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
WRAP_C_FUNCTION(CreateTextureFromSurface, 2, 0, {
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
})
WRAP_C_FUNCTION(DestroyTexture, 1, 0, {
  ObjTexture *texture = asTexture(argv[0]);
  if (texture->handle) {
    SDL_DestroyTexture(texture->handle);
    texture->handle = NULL;
  }
})
WRAP_SDL_FUNCTION(
    QueryTexture, 5, 0,
    SDL_QueryTexture(
        asTexture(argv[0])->handle,
        isNil(argv[1]) ? NULL : &asU32Cell(argv[1])->handle,
        isNil(argv[2]) ? NULL : &asIntCell(argv[2])->handle,
        isNil(argv[3]) ? NULL : &asIntCell(argv[3])->handle,
        isNil(argv[4]) ? NULL : &asIntCell(argv[4])->handle))
WRAP_C_FUNCTION(
    GetMouseState, 2, 0,
    *out = valNumber(SDL_GetMouseState(
        isNil(argv[0]) ? NULL : &asIntCell(argv[0])->handle,
        isNil(argv[1]) ? NULL : &asIntCell(argv[1])->handle)))
WRAP_C_FUNCTION(GetKeyboardState, 0, 0, {
  int n;
  const u8 *state = SDL_GetKeyboardState(&n);
  ObjConstU8Slice *slice = allocConstU8Slice();
  slice->handle.buffer = state;
  slice->handle.count = (size_t)n;
  *out = valConstU8Slice(slice);
})

/*
 * ████████ ████████ ███████
 *    ██       ██    ██
 *    ██       ██    █████
 *    ██       ██    ██
 *    ██       ██    ██
 * TTF module
 */
#if MTOTS_ENABLE_SDL_TTF

typedef struct ObjFont {
  ObjNative obj;
  TTF_Font *handle;
} ObjFont;
static void freeFont(ObjNative *n) {
  ObjFont *font = (ObjFont *)n;
  if (font->handle) {
    TTF_CloseFont(font->handle);
    font->handle = NULL;
  }
}
WRAP_C_TYPE_EX(Font, TTF_Font *, static, nopBlacken, freeFont)
static CFunction *FontMethods[] = {NULL};
static CFunction *FontStaticMethods[] = {NULL};

WRAP_C_FUNCTION_EX(Init, TTFInit, 0, 0, {
  if (TTF_Init() != 0) {
    return sdlError("TTF_Init");
  }
})

WRAP_C_FUNCTION_EX(Quit, TTFQuit, 0, 0, TTF_Quit())

WRAP_C_FUNCTION(OpenFont, 2, 0, {
  const char *file = asString(argv[0])->chars;
  int ptsize = asInt(argv[1]);
  TTF_Font *handle = TTF_OpenFont(file, ptsize);
  ObjFont *font;
  if (!handle) {
    return sdlError("TTF_OpenFont");
  }
  font = allocFont();
  font->handle = handle;
  *out = valFont(font);
})

WRAP_C_FUNCTION(CloseFont, 1, 0, {
  ObjFont *font = asFont(argv[0]);
  TTF_CloseFont(font->handle);
  font->handle = NULL;
})

WRAP_C_FUNCTION(RenderUTF8_Blended, 3, 0, {
  TTF_Font *font = asFont(argv[0])->handle;
  const char *text = asString(argv[1])->chars;
  SDL_Color fg = asColor(argv[2])->handle;
  SDL_Surface *handle = TTF_RenderUTF8_Blended(font, text, fg);
  ObjSurface *surface;
  if (!handle) {
    return sdlError("TTF_RenderUTF8_Blended");
  }
  surface = allocSurface();
  surface->handle = handle;
  *out = valSurface(surface);
})

WRAP_C_FUNCTION(RenderUTF8_Blended_Wrapped, 4, 0, {
  TTF_Font *font = asFont(argv[0])->handle;
  const char *text = asString(argv[1])->chars;
  SDL_Color fg = asColor(argv[2])->handle;
  Uint32 wrapLength = asU32(argv[3]);
  SDL_Surface *handle = TTF_RenderUTF8_Blended_Wrapped(font, text, fg, wrapLength);
  ObjSurface *surface;
  if (!handle) {
    return sdlError("TTF_RenderUTF8_Blended_Wrapped");
  }
  surface = allocSurface();
  surface->handle = handle;
  *out = valSurface(surface);
})

#endif

/*
 * ██ ███    ███  █████   ██████  ███████
 * ██ ████  ████ ██   ██ ██       ██
 * ██ ██ ████ ██ ███████ ██   ███ █████
 * ██ ██  ██  ██ ██   ██ ██    ██ ██
 * ██ ██      ██ ██   ██  ██████  ███████
 * Image module
 */
#if MTOTS_ENABLE_SDL_IMAGE

WRAP_C_FUNCTION_EX(Init, IMGInit, 1, 0, {
  u32 flags = asU32Bits(argv[0]);
  if ((IMG_Init(flags) & flags) != flags) {
    return sdlError("IMG_Init");
  }
})

WRAP_C_FUNCTION_EX(Quit, IMGQuit, 0, 0, IMG_Quit())

WRAP_C_FUNCTION_EX(Load, IMGLoad, 1, 0, {
  const char *file = asString(argv[0])->chars;
  SDL_Surface *handle = IMG_Load(file);
  ObjSurface *surface;
  if (!handle) {
    return sdlError("IMG_Load");
  }
  surface = allocSurface();
  surface->handle = handle;
  *out = valSurface(surface);
})

WRAP_C_FUNCTION_EX(Load_RW, IMGLoad_RW, 2, 0, {
  ObjRWops *src = asRWops(argv[0]);
  ubool freesrc = asBool(argv[1]);
  SDL_Surface *handle = IMG_Load_RW(src->handle, freesrc);
  ObjSurface *surface;
  if (freesrc) {
    src->handle = NULL;
    src->dataOwner = valNil();
  }
  if (!handle) {
    return sdlError("IMG_Load_RW");
  }
  surface = allocSurface();
  surface->handle = handle;
  *out = valSurface(surface);
})

WRAP_C_FUNCTION_EX(LoadTexture, IMGLoadTexture, 2, 0, {
  SDL_Renderer *renderer = asRenderer(argv[0])->handle;
  const char *file = asString(argv[1])->chars;
  SDL_Texture *handle = IMG_LoadTexture(renderer, file);
  ObjTexture *texture;
  if (!handle) {
    return sdlError("IMG_LoadTexture");
  }
  texture = allocTexture();
  texture->handle = handle;
  *out = valTexture(texture);
})

WRAP_C_FUNCTION_EX(LoadTexture_RW, IMGLoadTexture_RW, 3, 0, {
  SDL_Renderer *renderer = asRenderer(argv[0])->handle;
  ObjRWops *src = asRWops(argv[1]);
  ubool freesrc = asBool(argv[2]);
  SDL_Texture *handle = IMG_LoadTexture_RW(renderer, src->handle, freesrc);
  ObjTexture *texture;
  if (freesrc) {
    src->handle = NULL;
    src->dataOwner = valNil();
  }
  if (!handle) {
    return sdlError("IMG_LoadTexture_RW");
  }
  texture = allocTexture();
  texture->handle = handle;
  *out = valTexture(texture);
})

#endif

/*
 * ███    ███ ██ ██   ██ ███████ ██████
 * ████  ████ ██  ██ ██  ██      ██   ██
 * ██ ████ ██ ██   ███   █████   ██████
 * ██  ██  ██ ██  ██ ██  ██      ██   ██
 * ██      ██ ██ ██   ██ ███████ ██   ██
 */
#if MTOTS_ENABLE_SDL_MIXER

typedef struct ObjChunk {
  ObjNative obj;
  Mix_Chunk *handle;
} ObjChunk;
static void freeMixerChunk(ObjNative *n) {
  ObjChunk *chunk = (ObjChunk *)n;
  if (chunk->handle) {
    Mix_FreeChunk(chunk->handle);
    chunk->handle = NULL;
  }
}
WRAP_C_TYPE_EX(Chunk, Mix_Chunk *, static, nopBlacken, freeMixerChunk)
static CFunction *ChunkMethods[] = {NULL};
static CFunction *ChunkStaticMethods[] = {NULL};

WRAP_C_FUNCTION_EX(Init, MixInit, 1, 0, {
  u32 flags = asU32Bits(argv[0]);
  if ((Mix_Init(flags) & flags) != flags) {
    return sdlError("Mix_Init");
  }
})

WRAP_C_FUNCTION_EX(Quit, MixQuit, 0, 0, Mix_Quit())

WRAP_C_FUNCTION_EX(OpenAudio, MixOpenAudio, 4, 0, {
  int frequency = asInt(argv[0]);
  Uint16 format = asU16(argv[1]);
  int channels = asInt(argv[2]);
  int chunksize = asInt(argv[3]);
  if (Mix_OpenAudio(frequency, format, channels, chunksize) != 0) {
    return sdlError("Mix_OpenAudio");
  }
})

WRAP_C_FUNCTION_EX(CloseAudio, MixCloseAudio, 0, 0, Mix_CloseAudio())

WRAP_C_FUNCTION_EX(QuerySpec, MixQuerySpec, 3, 0,
                   *out = valNumber(Mix_QuerySpec(
                       &asIntCell(argv[0])->handle,
                       &asU16Cell(argv[1])->handle,
                       &asIntCell(argv[2])->handle)))

WRAP_C_FUNCTION_EX(AllocateChannels, MixAllocateChannels, 1, 0, {
  *out = valNumber(Mix_AllocateChannels(asNumber(argv[0])));
})

WRAP_C_FUNCTION_EX(LoadWAV, MixLoadWav, 1, 0, {
  const char *file = asString(argv[0])->chars;
  Mix_Chunk *handle = Mix_LoadWAV(file);
  ObjChunk *chunk;
  if (!handle) {
    return sdlError("Mix_LoadWAV");
  }
  chunk = allocChunk();
  chunk->handle = handle;
  *out = valChunk(chunk);
})

WRAP_C_FUNCTION_EX(LoadWAV_RW, MixLoadWAV_RW, 2, 0, {
  ObjRWops *src = asRWops(argv[0]);
  ubool freesrc = asBool(argv[1]);
  Mix_Chunk *handle = Mix_LoadWAV_RW(src->handle, freesrc);
  ObjChunk *chunk;
  if (!handle) {
    return sdlError("Mix_LoadWAV_RW");
  }
  chunk = allocChunk();
  chunk->handle = handle;
  *out = valChunk(chunk);
})

WRAP_C_FUNCTION_EX(FreeChunk, MixFreeChunk, 1, 0, {
  ObjChunk *chunk = asChunk(argv[0]);
  if (chunk->handle) {
    Mix_FreeChunk(chunk->handle);
    chunk->handle = NULL;
  }
})

WRAP_C_FUNCTION_EX(PlayChannel, MixPlayChannel, 3, 0, {
  int channel = asInt(argv[0]);
  ObjChunk *chunk = asChunk(argv[1]);
  int loops = asInt(argv[2]);
  int actualChannel = Mix_PlayChannel(channel, chunk->handle, loops);
  if (actualChannel < 0) {
    return sdlError("Mix_PlayChannel");
  }
  *out = valNumber(actualChannel);
})

WRAP_C_FUNCTION_EX(Playing, MixPlaying, 1, 0,
                   *out = valBool(Mix_Playing(asInt(argv[0]))))

WRAP_C_FUNCTION_EX(Pause, MixPause, 1, 0, Mix_Pause(asInt(argv[0])))

WRAP_C_FUNCTION_EX(Paused, MixPaused, 1, 0,
                   *out = valNumber(Mix_Paused(asInt(argv[0]))))

WRAP_C_FUNCTION_EX(Resume, MixResume, 1, 0, Mix_Resume(asInt(argv[0])))

#endif

/*
 * ███    ███  ██████  ██████  ██    ██ ██      ███████
 * ████  ████ ██    ██ ██   ██ ██    ██ ██      ██
 * ██ ████ ██ ██    ██ ██   ██ ██    ██ ██      █████
 * ██  ██  ██ ██    ██ ██   ██ ██    ██ ██      ██
 * ██      ██  ██████  ██████   ██████  ███████ ███████
 * module impls
 */

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcInit,
      &funcQuit,
      &funcPollEvent,
      &funcDelay,
      &funcGetPerformanceCounter,
      &funcGetPerformanceFrequency,
      &funcRWFromFile,
      &funcRWFromString,
      &funcRWFromBuffer,
      &funcRWclose,
      &funcRWsize,
      &funcCreateWindowAndRenderer,
      &funcSetWindowFullscreen,
      &funcSetWindowOpacity,
      &funcSetWindowPosition,
      &funcSetWindowResizable,
      &funcSetWindowSize,
      &funcSetWindowTitle,
      &funcSetRenderDrawColor,
      &funcRenderClear,
      &funcRenderFillRect,
      &funcRenderCopy,
      &funcRenderPresent,
      &funcRenderGetViewport,
      &funcCreateTextureFromSurface,
      &funcDestroyTexture,
      &funcQueryTexture,
      &funcGetMouseState,
      &funcGetKeyboardState,
      NULL,
  };

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
  ADD_TYPE_TO_MODULE(RWops);

  WRAP_CONST(QUIT, SDL_QUIT);
  WRAP_CONST(QUIT, SDL_QUIT);
  WRAP_CONST(INIT_TIMER, SDL_INIT_TIMER);
  WRAP_CONST(INIT_AUDIO, SDL_INIT_AUDIO);
  WRAP_CONST(INIT_VIDEO, SDL_INIT_VIDEO);
  WRAP_CONST(INIT_JOYSTICK, SDL_INIT_JOYSTICK);
  WRAP_CONST(INIT_HAPTIC, SDL_INIT_HAPTIC);
  WRAP_CONST(INIT_GAMECONTROLLER, SDL_INIT_GAMECONTROLLER);
  WRAP_CONST(INIT_EVENTS, SDL_INIT_EVENTS);
  WRAP_CONST(INIT_EVERYTHING, SDL_INIT_EVERYTHING);
  WRAP_CONST(WINDOW_FULLSCREEN, SDL_WINDOW_FULLSCREEN);
  WRAP_CONST(WINDOW_FULLSCREEN_DESKTOP, SDL_WINDOW_FULLSCREEN_DESKTOP);
  WRAP_CONST(WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  WRAP_CONST(WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED);

  WRAP_CONST(BUTTON_LMASK, SDL_BUTTON_LMASK);
  WRAP_CONST(BUTTON_MMASK, SDL_BUTTON_MMASK);
  WRAP_CONST(BUTTON_RMASK, SDL_BUTTON_RMASK);
  WRAP_CONST(BUTTON_X1MASK, SDL_BUTTON_X1MASK);
  WRAP_CONST(BUTTON_X2MASK, SDL_BUTTON_X2MASK);

  WRAP_CONST(SCANCODE_A, SDL_SCANCODE_A);
  WRAP_CONST(SCANCODE_B, SDL_SCANCODE_B);
  WRAP_CONST(SCANCODE_C, SDL_SCANCODE_C);
  WRAP_CONST(SCANCODE_D, SDL_SCANCODE_D);
  WRAP_CONST(SCANCODE_E, SDL_SCANCODE_E);
  WRAP_CONST(SCANCODE_F, SDL_SCANCODE_F);
  WRAP_CONST(SCANCODE_G, SDL_SCANCODE_G);
  WRAP_CONST(SCANCODE_H, SDL_SCANCODE_H);
  WRAP_CONST(SCANCODE_I, SDL_SCANCODE_I);
  WRAP_CONST(SCANCODE_J, SDL_SCANCODE_J);
  WRAP_CONST(SCANCODE_K, SDL_SCANCODE_K);
  WRAP_CONST(SCANCODE_L, SDL_SCANCODE_L);
  WRAP_CONST(SCANCODE_M, SDL_SCANCODE_M);
  WRAP_CONST(SCANCODE_N, SDL_SCANCODE_N);
  WRAP_CONST(SCANCODE_O, SDL_SCANCODE_O);
  WRAP_CONST(SCANCODE_P, SDL_SCANCODE_P);
  WRAP_CONST(SCANCODE_Q, SDL_SCANCODE_Q);
  WRAP_CONST(SCANCODE_R, SDL_SCANCODE_R);
  WRAP_CONST(SCANCODE_S, SDL_SCANCODE_S);
  WRAP_CONST(SCANCODE_T, SDL_SCANCODE_T);
  WRAP_CONST(SCANCODE_U, SDL_SCANCODE_U);
  WRAP_CONST(SCANCODE_V, SDL_SCANCODE_V);
  WRAP_CONST(SCANCODE_W, SDL_SCANCODE_W);
  WRAP_CONST(SCANCODE_X, SDL_SCANCODE_X);
  WRAP_CONST(SCANCODE_Y, SDL_SCANCODE_Y);
  WRAP_CONST(SCANCODE_Z, SDL_SCANCODE_Z);
  WRAP_CONST(SCANCODE_1, SDL_SCANCODE_1);
  WRAP_CONST(SCANCODE_2, SDL_SCANCODE_2);
  WRAP_CONST(SCANCODE_3, SDL_SCANCODE_3);
  WRAP_CONST(SCANCODE_4, SDL_SCANCODE_4);
  WRAP_CONST(SCANCODE_5, SDL_SCANCODE_5);
  WRAP_CONST(SCANCODE_6, SDL_SCANCODE_6);
  WRAP_CONST(SCANCODE_7, SDL_SCANCODE_7);
  WRAP_CONST(SCANCODE_8, SDL_SCANCODE_8);
  WRAP_CONST(SCANCODE_9, SDL_SCANCODE_9);
  WRAP_CONST(SCANCODE_0, SDL_SCANCODE_0);
  WRAP_CONST(SCANCODE_RETURN, SDL_SCANCODE_RETURN);
  WRAP_CONST(SCANCODE_ESCAPE, SDL_SCANCODE_ESCAPE);
  WRAP_CONST(SCANCODE_BACKSPACE, SDL_SCANCODE_BACKSPACE);
  WRAP_CONST(SCANCODE_TAB, SDL_SCANCODE_TAB);
  WRAP_CONST(SCANCODE_SPACE, SDL_SCANCODE_SPACE);
  WRAP_CONST(SCANCODE_MINUS, SDL_SCANCODE_MINUS);
  WRAP_CONST(SCANCODE_EQUALS, SDL_SCANCODE_EQUALS);
  WRAP_CONST(SCANCODE_LEFTBRACKET, SDL_SCANCODE_LEFTBRACKET);
  WRAP_CONST(SCANCODE_RIGHTBRACKET, SDL_SCANCODE_RIGHTBRACKET);
  WRAP_CONST(SCANCODE_BACKSLASH, SDL_SCANCODE_BACKSLASH);
  WRAP_CONST(SCANCODE_NONUSHASH, SDL_SCANCODE_NONUSHASH);
  WRAP_CONST(SCANCODE_SEMICOLON, SDL_SCANCODE_SEMICOLON);
  WRAP_CONST(SCANCODE_APOSTROPHE, SDL_SCANCODE_APOSTROPHE);
  WRAP_CONST(SCANCODE_GRAVE, SDL_SCANCODE_GRAVE);
  WRAP_CONST(SCANCODE_COMMA, SDL_SCANCODE_COMMA);
  WRAP_CONST(SCANCODE_PERIOD, SDL_SCANCODE_PERIOD);
  WRAP_CONST(SCANCODE_SLASH, SDL_SCANCODE_SLASH);
  WRAP_CONST(SCANCODE_CAPSLOCK, SDL_SCANCODE_CAPSLOCK);
  WRAP_CONST(SCANCODE_F1, SDL_SCANCODE_F1);
  WRAP_CONST(SCANCODE_F2, SDL_SCANCODE_F2);
  WRAP_CONST(SCANCODE_F3, SDL_SCANCODE_F3);
  WRAP_CONST(SCANCODE_F4, SDL_SCANCODE_F4);
  WRAP_CONST(SCANCODE_F5, SDL_SCANCODE_F5);
  WRAP_CONST(SCANCODE_F6, SDL_SCANCODE_F6);
  WRAP_CONST(SCANCODE_F7, SDL_SCANCODE_F7);
  WRAP_CONST(SCANCODE_F8, SDL_SCANCODE_F8);
  WRAP_CONST(SCANCODE_F9, SDL_SCANCODE_F9);
  WRAP_CONST(SCANCODE_F10, SDL_SCANCODE_F10);
  WRAP_CONST(SCANCODE_F11, SDL_SCANCODE_F11);
  WRAP_CONST(SCANCODE_F12, SDL_SCANCODE_F12);
  WRAP_CONST(SCANCODE_PRINTSCREEN, SDL_SCANCODE_PRINTSCREEN);
  WRAP_CONST(SCANCODE_SCROLLLOCK, SDL_SCANCODE_SCROLLLOCK);
  WRAP_CONST(SCANCODE_PAUSE, SDL_SCANCODE_PAUSE);
  WRAP_CONST(SCANCODE_INSERT, SDL_SCANCODE_INSERT);
  WRAP_CONST(SCANCODE_HOME, SDL_SCANCODE_HOME);
  WRAP_CONST(SCANCODE_PAGEUP, SDL_SCANCODE_PAGEUP);
  WRAP_CONST(SCANCODE_DELETE, SDL_SCANCODE_DELETE);
  WRAP_CONST(SCANCODE_END, SDL_SCANCODE_END);
  WRAP_CONST(SCANCODE_PAGEDOWN, SDL_SCANCODE_PAGEDOWN);
  WRAP_CONST(SCANCODE_RIGHT, SDL_SCANCODE_RIGHT);
  WRAP_CONST(SCANCODE_LEFT, SDL_SCANCODE_LEFT);
  WRAP_CONST(SCANCODE_DOWN, SDL_SCANCODE_DOWN);
  WRAP_CONST(SCANCODE_UP, SDL_SCANCODE_UP);
  WRAP_CONST(SCANCODE_NUMLOCKCLEAR, SDL_SCANCODE_NUMLOCKCLEAR);
  WRAP_CONST(SCANCODE_KP_DIVIDE, SDL_SCANCODE_KP_DIVIDE);
  WRAP_CONST(SCANCODE_KP_MULTIPLY, SDL_SCANCODE_KP_MULTIPLY);
  WRAP_CONST(SCANCODE_KP_MINUS, SDL_SCANCODE_KP_MINUS);
  WRAP_CONST(SCANCODE_KP_PLUS, SDL_SCANCODE_KP_PLUS);
  WRAP_CONST(SCANCODE_KP_ENTER, SDL_SCANCODE_KP_ENTER);
  WRAP_CONST(SCANCODE_KP_1, SDL_SCANCODE_KP_1);
  WRAP_CONST(SCANCODE_KP_2, SDL_SCANCODE_KP_2);
  WRAP_CONST(SCANCODE_KP_3, SDL_SCANCODE_KP_3);
  WRAP_CONST(SCANCODE_KP_4, SDL_SCANCODE_KP_4);
  WRAP_CONST(SCANCODE_KP_5, SDL_SCANCODE_KP_5);
  WRAP_CONST(SCANCODE_KP_6, SDL_SCANCODE_KP_6);
  WRAP_CONST(SCANCODE_KP_7, SDL_SCANCODE_KP_7);
  WRAP_CONST(SCANCODE_KP_8, SDL_SCANCODE_KP_8);
  WRAP_CONST(SCANCODE_KP_9, SDL_SCANCODE_KP_9);
  WRAP_CONST(SCANCODE_KP_0, SDL_SCANCODE_KP_0);
  WRAP_CONST(SCANCODE_KP_PERIOD, SDL_SCANCODE_KP_PERIOD);
  WRAP_CONST(SCANCODE_LCTRL, SDL_SCANCODE_LCTRL);
  WRAP_CONST(SCANCODE_LSHIFT, SDL_SCANCODE_LSHIFT);
  WRAP_CONST(SCANCODE_LALT, SDL_SCANCODE_LALT);
  WRAP_CONST(SCANCODE_LGUI, SDL_SCANCODE_LGUI);
  WRAP_CONST(SCANCODE_RCTRL, SDL_SCANCODE_RCTRL);
  WRAP_CONST(SCANCODE_RSHIFT, SDL_SCANCODE_RSHIFT);
  WRAP_CONST(SCANCODE_RALT, SDL_SCANCODE_RALT);
  WRAP_CONST(SCANCODE_RGUI, SDL_SCANCODE_RGUI);

  WRAP_CONST(AUDIO_S16SYS, AUDIO_S16SYS);
  WRAP_CONST(AUDIO_F32SYS, AUDIO_F32SYS);

  return STATUS_OK;
}

static CFunction func = {impl, "sdl", 1};

#if MTOTS_ENABLE_SDL_TTF
static Status implSDLTTF(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcTTFInit,
      &funcTTFQuit,
      &funcOpenFont,
      &funcCloseFont,
      &funcRenderUTF8_Blended,
      &funcRenderUTF8_Blended_Wrapped,
      NULL,
  };
  moduleAddFunctions(module, functions);
  ADD_TYPE_TO_MODULE(Font);
  return STATUS_OK;
}
static CFunction funcSDLTTF = {implSDLTTF, "sdl.ttf", 1};
#endif

#if MTOTS_ENABLE_SDL_IMAGE
static Status implSDLImage(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcIMGInit,
      &funcIMGQuit,
      &funcIMGLoad,
      &funcIMGLoad_RW,
      &funcIMGLoadTexture,
      &funcIMGLoadTexture_RW,
      NULL,
  };
  moduleAddFunctions(module, functions);

  WRAP_CONST(INIT_JPG, IMG_INIT_JPG);
  WRAP_CONST(INIT_PNG, IMG_INIT_PNG);
  WRAP_CONST(INIT_TIF, IMG_INIT_TIF);
  WRAP_CONST(INIT_WEBP, IMG_INIT_WEBP);
  WRAP_CONST(INIT_JXL, IMG_INIT_JXL);
  WRAP_CONST(INIT_AVIF, IMG_INIT_AVIF);

  return STATUS_OK;
}
static CFunction funcSDLImage = {implSDLImage, "sdl.image", 1};
#endif

#if MTOTS_ENABLE_SDL_MIXER
static Status implSDLMixer(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcMixInit,
      &funcMixQuit,
      &funcMixOpenAudio,
      &funcMixCloseAudio,
      &funcMixQuerySpec,
      &funcMixAllocateChannels,
      &funcMixLoadWav,
      &funcMixLoadWAV_RW,
      &funcMixFreeChunk,
      &funcMixPlayChannel,
      &funcMixPlaying,
      &funcMixPause,
      &funcMixPaused,
      &funcMixResume,
      NULL,
  };
  moduleAddFunctions(module, functions);
  ADD_TYPE_TO_MODULE(Chunk);
  WRAP_CONST(INIT_FLAC, MIX_INIT_FLAC);
  WRAP_CONST(INIT_MOD, MIX_INIT_MOD);
  WRAP_CONST(INIT_MP3, MIX_INIT_MP3);
  WRAP_CONST(INIT_OGG, MIX_INIT_OGG);
  WRAP_CONST(INIT_MID, MIX_INIT_MID);
  WRAP_CONST(INIT_OPUS, MIX_INIT_OPUS);
  return STATUS_OK;
}
static CFunction funcSDLMixer = {implSDLMixer, "sdl.mixer", 1};
#endif

void addNativeModuleSDL(void) {
  addNativeModule(&func);

#if MTOTS_ENABLE_SDL_TTF
  addNativeModule(&funcSDLTTF);
#endif

#if MTOTS_ENABLE_SDL_IMAGE
  addNativeModule(&funcSDLImage);
#endif

#if MTOTS_ENABLE_SDL_MIXER
  addNativeModule(&funcSDLMixer);
#endif
}

#else

static Status impl(i16 argc, Value *argv, Value *out) {
  runtimeError("No SDL support");
  return STATUS_ERROR;
}

static CFunction func = {impl, "sdl", 1};

void addNativeModuleSDL(void) {
  addNativeModule(&func);
}
#endif

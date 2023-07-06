#include "mtots_m_gg.h"
#include "mtots_m_gg_scancode.h"
#include "mtots_m_gg_colors.h"
#include "mtots_sdl.h"

#include "mtots_vm.h"

#include "mtots_m_canvas.h"
#include "mtots_m_audio.h"
#include "mtots_m_fontrm.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>

#define DEFAULT_FRAMES_PER_SECOND 30
#define SCANCODE_KEY_COUNT 256

#define VOLUME_MAX         128
#define DEFAULT_VOLUME      50
#define CHANNEL_COUNT        8

#define DEFAULT_PEN_FONT_SIZE 32

/*
  * NOTE: SDL pixel format enums are counterintuitive to me.
  * SDL_PIXELFORMAT_RGBA8888 and SDL_PIXELFORMAT_ABGR8888 seem
  * to flip order based on endianness.
  *
  * Then SDL_PIXELFORMAT_RGBA32 flips between SDL_PIXELFORMAT_RGBA8888
  * and SDL_PIXELFORMAT_ABGR8888 based on endianness.
  */
#define PIXELFORMAT SDL_PIXELFORMAT_RGBA32

#define AS_WINDOW(v) ((ObjWindow*)AS_OBJ((v)))
#define AS_TEXTURE(v) ((ObjTexture*)AS_OBJ((v)))
#define AS_GEOMETRY(v) ((ObjGeometry*)AS_OBJ((v)))
#define AS_CLICK_EVENT(v) ((ObjClickEvent*)AS_OBJ((v)))
#define AS_KEY_EVENT(v) ((ObjKeyEvent*)AS_OBJ((v)))
#define AS_MOTION_EVENT(v) ((ObjMotionEvent*)AS_OBJ((v)))
#define IS_WINDOW(v) ((getNativeObjectDescriptor((v)) == &descriptorWindow))
#define IS_TEXTURE(v) ((getNativeObjectDescriptor((v)) == &descriptorTexture))
#define IS_GEOMETRY(v) ((getNativeObjectDescriptor((v)) == &descriptorGeometry))
#define IS_CLICK_EVENT(v) ((getNativeObjectDescriptor((v)) == &descriptorClickEvent))
#define IS_KEY_EVENT(v) ((getNativeObjectDescriptor((v)) == &descriptorKeyEvent))
#define IS_MOTION_EVENT(v) ((getNativeObjectDescriptor((v)) == &descriptorMotionEvent))

typedef struct ObjTexture ObjTexture;
typedef struct ObjClickEvent ObjClickEvent;
typedef struct ObjKeyEvent ObjKeyEvent;
typedef struct ObjMotionEvent ObjMotionEvent;

static ubool updateStreamingTexture(SDL_Texture *texture, ObjImage *image);

typedef struct ObjWindow {
  ObjNative obj;
  SDL_Window *handle;      /* SDL window*/
  Uint64 framesPerSecond;
  SDL_Renderer *renderer;
  Value onUpdate;
  Value onClick;
  Value onClickUp;
  Value onKeyDown;
  Value onKeyUp;
  Value onMotion;
  Color backgroundColor;
  ObjCanvas *canvas;
  ObjTexture *canvasTexture;
  ObjMatrix *transform;
} ObjWindow;

struct ObjTexture {
  ObjNative obj;
  SDL_Texture *handle;
  ObjWindow *window;
  u32 width, height;
  ObjImage *image;       /* for streaming textures */
};

typedef struct ObjGeometry {
  ObjNative obj;
  ObjWindow *window;
  u32 vertexCount, indexCount;
  SDL_Vertex *vertices;         /* SDL vertices used for rendering */
  u32 *indices;
  ObjTexture *texture;
  Vector *vectors;              /* original points */
  ObjMatrix *transform;         /* transform on vectors to determine vertices */
} ObjGeometry;

struct ObjClickEvent {
  ObjNative obj;
  i32 x;
  i32 y;
  u8 button;
};

struct ObjKeyEvent {
  ObjNative obj;
  i32 scancode;
  String *key;
  ubool repeat;
};

struct ObjMotionEvent {
  ObjNative obj;
  i32 x;
  i32 y;
  i32 dx;
  i32 dy;
};

typedef struct AudioChannel {
  size_t sampleCount, currentSample, repeats;
  u16 volume;
  i16 *data;
  ubool pause;
} AudioChannel;

static String *buttonString;
static String *keyString;
static String *scancodeString;
static String *repeatString;
static String *dxString;
static String *dyString;
static String *transformString;
static String *scancodeKeys[SCANCODE_KEY_COUNT];
static ObjClickEvent *clickEvent;
static ObjKeyEvent *keyEvent;
static ObjMotionEvent *motionEvent;
static AudioChannel audioChannels[CHANNEL_COUNT];
static SDL_mutex *audioMutex;
static SDL_AudioDeviceID audioDevice;
static ObjWindow *activeWindow;
static ObjModule *ggModule;

static Value WINDOW_VAL(ObjWindow *window) {
  return OBJ_VAL_EXPLICIT((Obj*)window);
}

static Value TEXTURE_VAL(ObjTexture *texture) {
  return OBJ_VAL_EXPLICIT((Obj*)texture);
}

static Value GEOMETRY_VAL(ObjGeometry *geometry) {
  return OBJ_VAL_EXPLICIT((Obj*)geometry);
}

static Value CLICK_EVENT_VAL(ObjClickEvent *clickEvent) {
  return OBJ_VAL_EXPLICIT((Obj*)clickEvent);
}

static Value KEY_EVENT_VAL(ObjKeyEvent *keyEvent) {
  return OBJ_VAL_EXPLICIT((Obj*)keyEvent);
}

static Value MOTION_EVENT_VAL(ObjMotionEvent *motionEvent) {
  return OBJ_VAL_EXPLICIT((Obj*)motionEvent);
}

static void blackenWindow(ObjNative *n) {
  ObjWindow *window = (ObjWindow*)n;
  markValue(window->onUpdate);
  markValue(window->onClick);
  markValue(window->onClickUp);
  markValue(window->onKeyDown);
  markValue(window->onKeyUp);
  markValue(window->onMotion);
  markObject((Obj*)window->canvas);
  markObject((Obj*)window->canvasTexture);
  markObject((Obj*)window->transform);
}

static void freeWindow(ObjNative *n) {
  ObjWindow *window = (ObjWindow*)n;
  SDL_DestroyRenderer(window->renderer);
  SDL_DestroyWindow(window->handle);
}

static void blackenTexture(ObjNative *n) {
  ObjTexture *texture = (ObjTexture*)n;
  markObject((Obj*)texture->window);
  markObject((Obj*)texture->image);
}

static void freeTexture(ObjNative *n) {
  ObjTexture *texture = (ObjTexture*)n;
  if (texture->handle) {
    SDL_DestroyTexture(texture->handle);
  }
}

static void blackenGeometry(ObjNative *n) {
  ObjGeometry *geo = (ObjGeometry*)n;
  markObject((Obj*)geo->window);
  markObject((Obj*)geo->texture);
  markObject((Obj*)geo->transform);
}

static void freeGeometry(ObjNative *n) {
  ObjGeometry *geo = (ObjGeometry*)n;
  FREE_ARRAY(SDL_Vertex, geo->vertices, geo->vertexCount);
  FREE_ARRAY(u32, geo->indices, geo->indexCount);
  FREE_ARRAY(Vector, geo->vectors, geo->vertexCount);
}

static NativeObjectDescriptor descriptorWindow = {
  blackenWindow, freeWindow, sizeof(ObjWindow), "Window"
};

static NativeObjectDescriptor descriptorTexture = {
  blackenTexture, freeTexture, sizeof(ObjTexture), "Texture"
};

static NativeObjectDescriptor descriptorGeometry = {
  blackenGeometry, freeGeometry, sizeof(ObjGeometry), "Geometry"
};

static NativeObjectDescriptor descriptorClickEvent = {
  nopBlacken, nopFree, sizeof(ObjClickEvent), "ClickEvent"
};

static NativeObjectDescriptor descriptorKeyEvent = {
  nopBlacken, nopFree, sizeof(ObjKeyEvent), "KeyEvent"
};

static NativeObjectDescriptor descriptorMotionEvent = {
  nopBlacken, nopFree, sizeof(ObjMotionEvent), "MotionEvent"
};

static void lockAudioMutex() {
  if (SDL_LockMutex(audioMutex) != 0) {
    panic("SDL_LockMutex(audioMutex) failed");
  }
}

static void unlockAudioMutex() {
  if (SDL_UnlockMutex(audioMutex) != 0) {
    panic("SDL_UnlockMutex(audioMutex) failed");
  }
}

static void checkChannel(size_t channelIndex) {
  if (channelIndex > CHANNEL_COUNT) {
    panic(
      "Invalid audio channel %lu (CHANNEL_COUNT=%d)",
      (unsigned long)channelIndex,
      CHANNEL_COUNT);
  }
}

static i16 clamp(i32 value) {
  return value < I16_MIN ? I16_MIN :
         value > I16_MAX ? I16_MAX : ((i16)value);
}

static void audioCallback(void *userData, Uint8 *stream, int byteLength) {
  lockAudioMutex();
  {
    size_t sampleCount = ((size_t)byteLength) / 4, i, j;
    i16 *dat = (i16*)stream;
    for (i = 0; i < sampleCount; i++) {
      i32 left = 0, right = 0;
      for (j = 0; j < CHANNEL_COUNT; j++) {
        AudioChannel *ch = audioChannels + j;
        if (ch->pause) {
          continue;
        }
        if (ch->currentSample >= ch->sampleCount && ch->repeats) {
          ch->currentSample = 0;
          ch->repeats--;
        }
        if (ch->currentSample < ch->sampleCount) {
          left   += ch->volume * ch->data[ch->currentSample * 2 + 0];
          right  += ch->volume * ch->data[ch->currentSample * 2 + 1];
          ch->currentSample++;
        }
      }
      dat[2 * i + 0] = clamp(left  / VOLUME_MAX);
      dat[2 * i + 1] = clamp(right / VOLUME_MAX);
    }
  }
  unlockAudioMutex();
}

static void loadAudio(ObjAudio *audio, size_t channelIndex) {
  checkChannel(channelIndex);
  lockAudioMutex();
  {
    AudioChannel *ch = audioChannels + channelIndex;
    ch->data = realloc(ch->data, audio->buffer.length);
    ch->sampleCount = audio->buffer.length / 4;
    ch->currentSample = ch->sampleCount;
    ch->volume = DEFAULT_VOLUME;
    ch->repeats = 0;
    ch->pause = 0;
    memcpy(ch->data, audio->buffer.data, audio->buffer.length);
  }
  unlockAudioMutex();
}

static void playAudio(size_t channelIndex, size_t repeats) {
  checkChannel(channelIndex);
  if (!audioDevice) {
    SDL_AudioSpec spec;
    spec.freq = 44100;
    spec.format = AUDIO_S16LSB;
    spec.channels = 2;
    spec.samples = 2048;
    spec.callback = audioCallback;
    spec.userdata = NULL;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!audioDevice) {
      panic("SDL_OpenAudioDevice: %s", SDL_GetError());
    }
    SDL_PauseAudioDevice(audioDevice, 0);
  }
  lockAudioMutex();
  {
    AudioChannel *ch = audioChannels + channelIndex;
    ch->currentSample = 0;
    ch->repeats = repeats;
    ch->pause = UFALSE;
  }
  unlockAudioMutex();
}

static void pauseAudio(size_t channelIndex, ubool pause) {
  checkChannel(channelIndex);
  lockAudioMutex();
  {
    AudioChannel *ch = audioChannels + channelIndex;
    ch->repeats = 0;
    ch->pause = pause;
  }
  unlockAudioMutex();
}

static void setAudioVolume(size_t channelIndex, u16 volume) {
  checkChannel(channelIndex);
  lockAudioMutex();
  if (volume > VOLUME_MAX) {
    volume = VOLUME_MAX;
  }
  {
    AudioChannel *ch = audioChannels + channelIndex;
    ch->volume = volume;
  }
  unlockAudioMutex();
}

static void setDrawColor(ObjWindow *window, Color color) {
  SDL_SetRenderDrawColor(
    window->renderer, color.red, color.green, color.blue, color.alpha);
}

static ubool newWindow(
    const char *title,
    i32 width,
    i32 height,
    Uint64 framesPerSecond,
    ObjWindow **out) {
  SDL_Window *handle;
  SDL_Renderer *renderer;
  ObjWindow *window;
  ubool gcPause;

  handle = SDL_CreateWindow(
    title,
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    width, height,
    0);
  if (!handle) {
    return sdlError("SDL_CreateWindow");
  }

  renderer = SDL_CreateRenderer(handle, -1,
    SDL_RENDERER_ACCELERATED|
    SDL_RENDERER_PRESENTVSYNC|
    SDL_RENDERER_TARGETTEXTURE);
  if (!renderer) {
    sdlError("SDL_CreateRenderer");
    SDL_DestroyWindow(handle);
    return UFALSE;
  }

  window = NEW_NATIVE(ObjWindow, &descriptorWindow);
  LOCAL_GC_PAUSE(gcPause);
  window->handle = handle;
  window->framesPerSecond = framesPerSecond;
  window->renderer = renderer;
  window->onUpdate = NIL_VAL();
  window->onClick = NIL_VAL();
  window->onClickUp = NIL_VAL();
  window->onKeyDown = NIL_VAL();
  window->onKeyUp = NIL_VAL();
  window->onMotion = NIL_VAL();
  window->backgroundColor = newColor(162, 136, 121, 255); /* MEDIUM_GREY */
  window->canvas = NULL;
  window->canvasTexture = NULL;
  window->transform = newIdentityMatrix();
  LOCAL_GC_UNPAUSE(gcPause);

  *out = window;
  return UTRUE;
}

static ubool mainLoopIteration(ObjWindow *mainWindow, ubool *quit) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        *quit = UTRUE;
        return UTRUE;
      case SDL_MOUSEBUTTONDOWN:
        if (!IS_NIL(mainWindow->onClick)) {
          clickEvent->x = event.button.x;
          clickEvent->y = event.button.y;
          clickEvent->button = event.button.button;
          push(mainWindow->onClick);
          push(CLICK_EVENT_VAL(clickEvent));
          if (!callFunction(1)) {
            return UFALSE;
          }
          pop();
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (!IS_NIL(mainWindow->onClickUp)) {
          clickEvent->x = event.button.x;
          clickEvent->y = event.button.y;
          clickEvent->button = event.button.button;
          push(mainWindow->onClickUp);
          push(CLICK_EVENT_VAL(clickEvent));
          if (!callFunction(1)) {
            return UFALSE;
          }
          pop();
        }
        break;
      case SDL_KEYDOWN:
        if (!IS_NIL(mainWindow->onKeyDown)) {
          i32 scancode = event.key.keysym.scancode;
          if (scancode < SCANCODE_KEY_COUNT) {
            String *key = scancodeKeys[scancode];
            if (key) {
              keyEvent->scancode = scancode;
              keyEvent->key = key;
              keyEvent->repeat = !!event.key.repeat;
              push(mainWindow->onKeyDown);
              push(KEY_EVENT_VAL(keyEvent));
              if (!callFunction(1)) {
                return UFALSE;
              }
              pop();
            }
          }
        }
        break;
      case SDL_KEYUP:
        if (!IS_NIL(mainWindow->onKeyUp)) {
          i32 scancode = event.key.keysym.scancode;
          if (scancode < SCANCODE_KEY_COUNT) {
            String *key = scancodeKeys[scancode];
            if (key) {
              keyEvent->scancode = scancode;
              keyEvent->key = key;
              keyEvent->repeat = !!event.key.repeat;
              push(mainWindow->onKeyUp);
              push(KEY_EVENT_VAL(keyEvent));
              if (!callFunction(1)) {
                return UFALSE;
              }
              pop();
            }
          }
        }
        break;
      case SDL_MOUSEMOTION:
        if (!IS_NIL(mainWindow->onMotion)) {
          motionEvent->x = event.motion.x;
          motionEvent->y = event.motion.y;
          motionEvent->dx = event.motion.xrel;
          motionEvent->dy = event.motion.yrel;
          push(mainWindow->onMotion);
          push(MOTION_EVENT_VAL(motionEvent));
          if (!callFunction(1)) {
            return UFALSE;
          }
          pop();
        }
        break;
    }
  }
  setDrawColor(mainWindow, mainWindow->backgroundColor);
  SDL_RenderClear(mainWindow->renderer);
  if (!IS_NIL(mainWindow->onUpdate)) {
    push(mainWindow->onUpdate);
    callFunction(0);
    pop();
  }
  if (mainWindow->canvasTexture) {
    if (!updateStreamingTexture(
        mainWindow->canvasTexture->handle,
        mainWindow->canvasTexture->image)) {
      return UFALSE;
    }
    if (SDL_RenderCopy(
        mainWindow->renderer,
        mainWindow->canvasTexture->handle,
        NULL,
        NULL) != 0) {
      return sdlError("SDL_RenderCopy");
    }
  }
  SDL_RenderPresent(mainWindow->renderer);
  return UTRUE;
}

#ifdef __EMSCRIPTEN__
static ObjWindow *mainWindowForEmscripten;
static void mainLoopIterationEmscripten() {
  ubool quit = UFALSE;
  if (!mainLoopIteration(mainWindowForEmscripten, &quit)) {
    panic("mainLoop: %s", getErrorString());
  }
  if (quit) {
    emscripten_cancel_main_loop();
  }
}
#endif


static ubool mainLoop(ObjWindow *mainWindow) {
#ifdef __EMSCRIPTEN__
  mainWindowForEmscripten = mainWindow;
  emscripten_set_main_loop(mainLoopIterationEmscripten, mainWindow->framesPerSecond, 1);
  return UTRUE;
#else
  Uint64 tick = 0;
  Uint64 framesPerSecond = mainWindow->framesPerSecond;
  Uint64 countPerSecond = SDL_GetPerformanceFrequency();
  Uint64 countPerFrame = countPerSecond / framesPerSecond;
  ubool quit = UFALSE;
  push(WINDOW_VAL(mainWindow));
  for(tick = 0;; tick++) {
    Uint64 startTime = SDL_GetPerformanceCounter(), endTime, elapsedTime;
    if (!mainLoopIteration(mainWindow, &quit)) {
      return UFALSE;
    }
    if (quit) {
      return UTRUE;
    }
    endTime = SDL_GetPerformanceCounter();
    elapsedTime = endTime - startTime;
    if (elapsedTime < countPerFrame) {
      Uint32 ms = (countPerFrame - elapsedTime) * 1000 / countPerSecond;
      SDL_Delay(ms);
    } else {
      eprintln(
        "WARNING: mainloop frame was delayed on tick %llu (%llu >= %llu, countPerSec=%llu)",
        tick, elapsedTime, countPerFrame, countPerSecond);
    }
  }
  return UTRUE;
#endif
}

static ubool newStaticTexture(ObjWindow *window, ObjImage *image, ObjTexture **out) {
  SDL_Surface *surface;
  SDL_Texture *handle;
  ObjTexture *texture;
  surface = SDL_CreateRGBSurfaceWithFormatFrom(
    image->pixels, image->width, image->height, 32, image->width * 4,
    PIXELFORMAT);
  if (!surface) {
    return sdlError("SDL_CreateRGBSurfaceWithFormatFrom");
  }
  handle = SDL_CreateTextureFromSurface(window->renderer, surface);
  if (!handle) {
    sdlError("SDL_CreateTextureFromSurface");
    SDL_FreeSurface(surface);
    return UFALSE;
  }
  SDL_FreeSurface(surface);
  texture = NEW_NATIVE(ObjTexture, &descriptorTexture);
  texture->handle = handle;
  texture->window = window;
  texture->width = image->width;
  texture->height = image->height;
  texture->image = NULL;
  *out = texture;
  return UTRUE;
}

static ubool newStreamingTexture(ObjWindow *window, ObjImage *image, ObjTexture **out) {
  ObjTexture *texture;
  SDL_Texture *handle;
  handle = SDL_CreateTexture(
    window->renderer,
    PIXELFORMAT,
    SDL_TEXTUREACCESS_STREAMING,
    image->width, image->height);
  if (!handle) {
    return sdlError("SDL_CreateTexture");
  }
  if (SDL_SetTextureBlendMode(handle, SDL_BLENDMODE_BLEND) != 0) {
    sdlError("SDL_SetTextureBlendMode");
    SDL_DestroyTexture(handle);
    return UFALSE;
  }
  if (!updateStreamingTexture(handle, image)) {
    SDL_DestroyTexture(handle);
    return UFALSE;
  }
  texture = NEW_NATIVE(ObjTexture, &descriptorTexture);
  texture->handle = handle;
  texture->window = window;
  texture->width = image->width;
  texture->height = image->height;
  texture->image = image;
  *out = texture;
  return UTRUE;
}

static ubool updateStreamingTexture(SDL_Texture *texture, ObjImage *image) {
  u8 *pixels;
  int pitch;
  size_t row;
  if (SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch) != 0) {
    return sdlError("SDL_LockTexture");
  }
  for (row = 0; row < image->height; row++, pixels += pitch) {
    memcpy(pixels, image->pixels + row * image->width, 4 * image->width);
  }
  SDL_UnlockTexture(texture);
  return UTRUE;
}

static ubool windowNewCanvas(ObjWindow *window, size_t width, size_t height) {
  ObjImage *image;
  ubool gcPause;

  window->canvas = NULL;
  window->canvasTexture = NULL;
  image = newImage(width, height);

  LOCAL_GC_PAUSE(gcPause);

  memset(image->pixels, 0, sizeof(Color) * width * height);
  if (!newStreamingTexture(window, image, &window->canvasTexture)) {
    LOCAL_GC_UNPAUSE(gcPause);
    return UFALSE;
  }
  window->canvas = newCanvas(image);

  LOCAL_GC_UNPAUSE(gcPause);

  return UTRUE;
}

static SDL_Color toSDLColor(Color color) {
  SDL_Color sc;
  sc.r = color.red;
  sc.g = color.green;
  sc.b = color.blue;
  sc.a = color.alpha;
  return sc;
}

static ObjGeometry *newGeometry(ObjWindow *window, u32 vertexCount, u32 indexCount) {
  ObjGeometry *geo = NEW_NATIVE(ObjGeometry, &descriptorGeometry);
  ubool gcPause;
  LOCAL_GC_PAUSE(gcPause);
  geo->window = window;
  geo->vertexCount = vertexCount;
  geo->indexCount = indexCount;
  geo->vertices = ALLOCATE(SDL_Vertex, vertexCount);
  geo->indices = ALLOCATE(u32, indexCount);
  geo->texture = NULL;
  geo->vectors = ALLOCATE(Vector, vertexCount);
  geo->transform = newIdentityMatrix();
  LOCAL_GC_UNPAUSE(gcPause);
  return geo;
}

static ubool newPolygonGeometry(
    ObjWindow *window,
    ObjList *points,
    ObjList *colors,
    ObjTexture *texture,
    ObjList *textureCoordinates,
    ObjGeometry **out) {
  u32 vertexCount = points->length;
  u32 indexCount;
  size_t i;
  ObjGeometry *geo;
  if (vertexCount < 3) {
    runtimeError("Polygons require at least 3 vertices but got %d", (int)vertexCount);
    return UFALSE;
  }
  if (colors && colors->length != 1 && colors->length != vertexCount) {
    runtimeError(
      "Polygons require that the colors list length match the point list length "
      "but got %d points and %d colors", (int)vertexCount, (int)colors->length);
    return UFALSE;
  }
  if (textureCoordinates && textureCoordinates->length != vertexCount) {
    runtimeError(
      "Polygons require that the textureCoordinates list length match the point "
      "length but got %d points and %d texture coordinates", (int)vertexCount,
      (int)textureCoordinates->length);
    return UFALSE;
  }
  indexCount = (vertexCount - 2) * 3;
  geo = newGeometry(window, vertexCount, indexCount);
  geo->texture = texture;
  for (i = 0; i < vertexCount; i++) {
    Vector point;
    SDL_Color color;
    SDL_FPoint texCoord;
    if (!IS_VECTOR(points->buffer[i])) {
      runtimeError("Expected Vector but got %s", getKindName(points->buffer[i]));
      return UFALSE;
    }
    point = AS_VECTOR(points->buffer[i]);
    if (colors) {
      size_t colorIndex = colors->length == 1 ? 0 : i;
      if (!IS_COLOR(colors->buffer[colorIndex])) {
        runtimeError("Expected Color but got %s", getKindName(colors->buffer[colorIndex]));
        return UFALSE;
      }
      color = toSDLColor(AS_COLOR(colors->buffer[colorIndex]));
    } else {
      color = toSDLColor(newColor(255, 255, 255, 255));
    }
    if (textureCoordinates) {
      Vector vec;
      if (!IS_VECTOR(textureCoordinates->buffer[i])) {
        runtimeError("Expected Vector but got %s", getKindName(
          textureCoordinates->buffer[i]));
        return UFALSE;
      }
      vec = AS_VECTOR(textureCoordinates->buffer[i]);
      texCoord.x = vec.x;
      texCoord.y = vec.y;
    } else {
      texCoord.x = 0;
      texCoord.y = 0;
      if (texture) {
        switch (i) {
          case 0:
            break;
          case 1:
            texCoord.x = 1;
            break;
          case 2:
            texCoord.x = 1;
            texCoord.y = 1;
            break;
          case 3:
            texCoord.y = 1;
            break;
        }
      }
    }
    geo->vertices[i].color = color;
    geo->vertices[i].tex_coord = texCoord;
    geo->vertices[i].position.x = 0; /* position should be overwritten on blit */
    geo->vertices[i].position.y = 0;
    geo->vectors[i] = point;
  }
  for (i = 0; i < vertexCount - 2; i++) { /* triangle fan */
    geo->indices[3 * i + 0] = 0;
    geo->indices[3 * i + 1] = i + 1;
    geo->indices[3 * i + 2] = i + 2;
  }
  *out = geo;
  return UTRUE;
}

/* Update the coordinates of the geometry based on vectors and
  * the transform matrix.
  *
  * We take into account:
  *   1. the transform matrix of the window, and
  *   2. the transform matrix of the geometry */
static ubool geometryUpdateVertexPositions(ObjGeometry *geo) {
  size_t i;
  Matrix transform = geo->window->transform->handle;
  matrixIMul(&transform, &geo->transform->handle);
  for (i = 0; i < geo->vertexCount; i++) {
    Vector vec = matrixApply(&transform, geo->vectors[i]);
    geo->vertices[i].position.x = vec.x;
    geo->vertices[i].position.y = vec.y;
  }
  return UTRUE;
}

static ubool geometryBlit(ObjGeometry *geo) {
  if (!geometryUpdateVertexPositions(geo)) {
    return UFALSE;
  }

  /* Actually render the geometry */
  if (SDL_RenderGeometry(
      geo->window->renderer,
      geo->texture ? geo->texture->handle : NULL,
      geo->vertices,
      (int)geo->vertexCount,
      (const i32*)geo->indices,
      (int)geo->indexCount) != 0) {
    return sdlError("SDL_RenderGeometry");
  }

  return UTRUE;
}

static ubool implMainLoop(i16 argc, Value *args, Value *out) {
  vm.runOnFinish = NULL;
  return mainLoop(activeWindow);
}

static CFunction funcMainLoop = { implMainLoop, "mainLoop" };

static ubool implWindowStaticCall(i16 argc, Value *args, Value *out) {
  const char *title = argc > 0 ? AS_STRING(args[0])->chars : "";
  i32 width = argc > 1 ? AS_I32(args[1]) : 0;
  i32 height = argc > 2 ? AS_I32(args[2]) : 0;
  u32 framesPerSecond = argc > 3 ? AS_U32(args[3]) : DEFAULT_FRAMES_PER_SECOND;
  if (activeWindow) {
    runtimeError("Only one window is allowed at a time");
    return UFALSE;
  }
  if (!newWindow(title, width, height, framesPerSecond, &activeWindow)) {
    return UFALSE;
  }
  vm.runOnFinish = &funcMainLoop;

  *out = WINDOW_VAL(activeWindow);
  return UTRUE;
}

static TypePattern argsWindowStaticCall[] = {
  { TYPE_PATTERN_STRING },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
};

static CFunction funcWindowStaticCall = {
  implWindowStaticCall, "__call__", 0, 4, argsWindowStaticCall,
};

static ubool implWindowGetattr(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == transformString) {
    *out = MATRIX_VAL(window->transform);
  } else {
    runtimeError("Field %s not found on Window", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcWindowGetattr = {
  implWindowGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implWindowMainLoop(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  vm.runOnFinish = NULL;
  return mainLoop(window);
}

static CFunction funcWindowMainLoop = { implWindowMainLoop, "mainLoop" };

static ubool implWindowSetTitle(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  const char *title = AS_STRING(args[0])->chars;
  SDL_SetWindowTitle(window->handle, title);
  return UTRUE;
}

static CFunction funcWindowSetTitle = {
  implWindowSetTitle, "setTitle", 1, 0, argsStrings
};

static ubool implWindowSetBackgroundColor(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  Color color = AS_COLOR(args[0]);
  window->backgroundColor = color;
  return UTRUE;
}

static TypePattern argsWindowSetBackgroundColor[] = {
  { TYPE_PATTERN_COLOR },
};

static CFunction funcWindowSetBackgroundColor = {
  implWindowSetBackgroundColor, "setBackgroundColor", 1, 0, argsWindowSetBackgroundColor
};

static ubool implWindowOnUpdate(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  window->onUpdate = args[0];
  return UTRUE;
}

static CFunction funcWindowOnUpdate = { implWindowOnUpdate, "onUpdate", 1 };

static ubool implWindowOnClick(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  window->onClick = args[0];
  return UTRUE;
}

static CFunction funcWindowOnClick = { implWindowOnClick, "onClick", 1 };

static ubool implWindowOnClickUp(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  window->onClickUp = args[0];
  return UTRUE;
}

static CFunction funcWindowOnClickUp = { implWindowOnClickUp, "onClickUp", 1 };

static ubool implWindowOnKeyDown(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  window->onKeyDown = args[0];
  return UTRUE;
}

static CFunction funcWindowOnKeyDown = { implWindowOnKeyDown, "onKeyDown", 1 };

static ubool implWindowOnKeyUp(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  window->onKeyUp = args[0];
  return UTRUE;
}

static CFunction funcWindowOnKeyUp = { implWindowOnKeyUp, "onKeyUp", 1 };

static ubool implWindowOnMotion(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  window->onMotion = args[0];
  return UTRUE;
}

static CFunction funcWindowOnMotion = { implWindowOnMotion, "onMotion", 1 };

static ubool implWindowGetCanvas(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  if (!window->canvas) {
    int rw, rh;
    if (SDL_GetRendererOutputSize(window->renderer, &rw, &rh) != 0) {
      return sdlError("SDL_GetRendererOutputSize");
    }
    if (!windowNewCanvas(window, rw, rh)) {
      return UFALSE;
    }
  }
  *out = CANVAS_VAL(window->canvas);
  return UTRUE;
}

static CFunction funcWindowGetCanvas = { implWindowGetCanvas, "getCanvas" };

static ubool implWindowNewCanvas(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  i32 width = argc > 0 ? AS_I32(args[0]) : -1;
  i32 height = argc > 1 ? AS_I32(args[1]) : -1;
  if (width < 0 || height < 0) {
    int rw, rh;
    if (SDL_GetRendererOutputSize(window->renderer, &rw, &rh) != 0) {
      return sdlError("SDL_GetRendererOutputSize");
    }
    if (width < 0) {
      width = rw;
    }
    if (height < 0) {
      height = rh;
    }
  }
  if (!windowNewCanvas(window, width, height)) {
    return UFALSE;
  }
  *out = CANVAS_VAL(window->canvas);
  return UTRUE;
}

static CFunction funcWindowNewCanvas = {
  implWindowNewCanvas, "newCanvas", 0, 2, argsNumbers
};

static ubool implWindowClear(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  setDrawColor(window, AS_COLOR(args[0]));
  SDL_RenderClear(window->renderer);
  return UTRUE;
}

static TypePattern argsWindowClear[] = {
  { TYPE_PATTERN_COLOR },
};

static CFunction funcWindowClear = {
  implWindowClear, "clear", 1, 0, argsWindowClear
};

static ubool implWindowNewTexture(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  ObjImage *image = AS_IMAGE(args[0]);
  ubool streaming = argc > 1 && !IS_NIL(args[1]) ? AS_BOOL(args[1]) : UFALSE;
  ObjTexture *texture;

  if (streaming) {
    if (!newStreamingTexture(window, image, &texture)) {
      return UFALSE;
    }
  } else {
    if (!newStaticTexture(window, image, &texture)) {
      return UFALSE;
    }
  }
  *out = TEXTURE_VAL(texture);
  return UTRUE;
}

static TypePattern argsWindowNewTexture[] = {
  { TYPE_PATTERN_NATIVE, &descriptorImage },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcWindowNewTexture = {
  implWindowNewTexture, "newTexture", 1, 2, argsWindowNewTexture,
};

static ubool implWindowNewPolygon(i16 argc, Value *args, Value *out) {
  ObjWindow *window = AS_WINDOW(args[-1]);
  ObjList *points = AS_LIST(args[0]);
  ObjList *colors = argc > 1 && !IS_NIL(args[1]) ? AS_LIST(args[1]) : NULL;
  ObjTexture *texture = argc > 2 && !IS_NIL(args[2]) ? AS_TEXTURE(args[2]) : NULL;
  ObjList *textureCoordinates = argc > 3 && !IS_NIL(args[3]) ? AS_LIST(args[3]) : NULL;
  ObjGeometry *geo;
  if (!newPolygonGeometry(window, points, colors, texture, textureCoordinates, &geo)) {
    return UFALSE;
  }
  *out = GEOMETRY_VAL(geo);
  return UTRUE;
}

static TypePattern argsWindowNewPolygon[] = {
  { TYPE_PATTERN_LIST },
  { TYPE_PATTERN_LIST_OR_NIL },
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorTexture },
  { TYPE_PATTERN_LIST_OR_NIL },
};

static CFunction funcWindowNewPolygon = {
  implWindowNewPolygon, "newPolygon", 1, 4, argsWindowNewPolygon
};

static ubool implTextureGetattr(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.widthString) {
    *out = NUMBER_VAL(texture->width);
  } else if (name == vm.heightString) {
    *out = NUMBER_VAL(texture->height);
  } else {
    runtimeError("Field %s not found on Texture", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcTextureGetattr = {
  implTextureGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implTextureBlit(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  Vector dstPos = AS_VECTOR(args[0]);
  SDL_FRect dstRect;
  dstRect.x = dstPos.x;
  dstRect.y = dstPos.y;
  dstRect.w = texture->width;
  dstRect.h = texture->height;
  if (SDL_RenderCopyF(
      texture->window->renderer,
      texture->handle,
      NULL, &dstRect) != 0) {
    return sdlError("SDL_RenderCopyF");
  }
  return UTRUE;
}

static TypePattern argsTextureBlit[] = {
  { TYPE_PATTERN_VECTOR },
};

static CFunction funcTextureBlit = {
  implTextureBlit, "blit", 1, 0, argsTextureBlit,
};

static ubool implTextureIsStreaming(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  *out = BOOL_VAL(!!texture->image);
  return UTRUE;
}

static CFunction funcTextureIsStreaming = {
  implTextureIsStreaming, "isStreaming",
};

static ubool implTextureUpdate(i16 argc, Value *args, Value *out) {
  ObjTexture *texture = AS_TEXTURE(args[-1]);
  if (!texture->image) {
    runtimeError("update() called on a non-streaming Texture");
    return UFALSE;
  }
  if (!updateStreamingTexture(texture->handle, texture->image)) {
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcTextureUpdate = { implTextureUpdate, "update" };

static ubool implGeometryGetattr(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == transformString) {
    *out = MATRIX_VAL(geo->transform);
  } else {
    runtimeError("Field %s not found in Geometry", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcGeometryGetattr = {
  implGeometryGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implGeometryBlit(i16 argc, Value *args, Value *out) {
  return geometryBlit(AS_GEOMETRY(args[-1]));
}

static CFunction funcGeometryBlit = { implGeometryBlit, "blit" };

static ubool implGeometrySetColor(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  SDL_Color color = toSDLColor(AS_COLOR(args[0]));
  u32 i;
  for (i = 0; i < geo->vertexCount; i++) {
    geo->vertices[i].color = color;
  }
  return UTRUE;
}

static TypePattern argsGeometrySetColor[] = {
  { TYPE_PATTERN_COLOR },
};

static CFunction funcGeometrySetColor = {
  implGeometrySetColor, "setColor", 1, 0, argsGeometrySetColor
};

static ubool implGeometrySetVertexColor(i16 argc, Value *args, Value *out) {
  ObjGeometry *geo = AS_GEOMETRY(args[-1]);
  size_t vi = AS_INDEX(args[0], (size_t)geo->vertexCount);
  geo->vertices[vi].color = toSDLColor(AS_COLOR(args[1]));
  return UTRUE;
}

static TypePattern argsGeometrySetVertexColor[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_COLOR },
};

static CFunction funcGeometrySetVertexColor = {
  implGeometrySetVertexColor, "setVertexColor", 2, 0, argsGeometrySetVertexColor
};

static ubool implClickEventGetattr(i16 argc, Value *args, Value *out) {
  ObjClickEvent *clickEvent = AS_CLICK_EVENT(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.xString) {
    *out = NUMBER_VAL(clickEvent->x);
  } else if (name == vm.yString) {
    *out = NUMBER_VAL(clickEvent->y);
  } else if (name == buttonString) {
    *out = NUMBER_VAL(clickEvent->button);
  } else {
    runtimeError("Field %s not found in ClickEvent", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcClickEventGetattr = {
  implClickEventGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implKeyEventGetattr(i16 argc, Value *args, Value *out) {
  ObjKeyEvent *keyEvent = AS_KEY_EVENT(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == keyString) {
    *out = STRING_VAL(keyEvent->key);
  } else if (name == scancodeString) {
    *out = NUMBER_VAL(keyEvent->scancode);
  } else if (name == repeatString) {
    *out = BOOL_VAL(keyEvent->repeat);
  } else {
    runtimeError("Field %s not found in KeyEvent", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcKeyEventGetattr = {
  implKeyEventGetattr, "__getattr__", 1, 0, argsStrings
};

static ubool implMotionEventGetattr(i16 argc, Value *args, Value *out) {
  ObjMotionEvent *motionEvent = AS_MOTION_EVENT(args[-1]);
  String *name = AS_STRING(args[0]);
  if (name == vm.xString) {
    *out = NUMBER_VAL(motionEvent->x);
  } else if (name == vm.yString) {
    *out = NUMBER_VAL(motionEvent->y);
  } else if (name == dxString) {
    *out = NUMBER_VAL(motionEvent->dx);
  } else if (name == dyString) {
    *out = NUMBER_VAL(motionEvent->dy);
  } else {
    runtimeError("Field %s not found in MotionEvent", name->chars);
    return UFALSE;
  }
  return UTRUE;
}

static CFunction funcMotionEventGetattr = {
  implMotionEventGetattr, "__getattr__", 1, 0, argsStrings,
};

static ubool implLoadAudio(i16 argc, Value *args, Value *out) {
  ObjAudio *audio = AS_AUDIO(args[0]);
  size_t channel = argc > 1 ? AS_INDEX(args[1], CHANNEL_COUNT) : 0;
  loadAudio(audio, channel);
  return UTRUE;
}

static TypePattern argsLoadAudio[] = {
  { TYPE_PATTERN_NATIVE, &descriptorAudio },
  { TYPE_PATTERN_NUMBER },
};

static CFunction funcLoadAudio = {
  implLoadAudio, "loadAudio", 1, 2, argsLoadAudio
};

static ubool implPlayAudio(i16 argc, Value *args, Value *out) {
  size_t channel = argc > 0 ? AS_INDEX(args[0], CHANNEL_COUNT) : 0;
  i32 repeats = argc > 1 ? AS_I32(args[1]) : 0;
  if (repeats < 0) {
    repeats = I32_MAX;
  }
  playAudio(channel, repeats);
  return UTRUE;
}

static CFunction funcPlayAudio = { implPlayAudio, "playAudio", 0, 2, argsNumbers };

static ubool implPauseAudio(i16 argc, Value *args, Value *out) {
  size_t channel = argc > 0 ? AS_INDEX(args[0], CHANNEL_COUNT) : 0;
  ubool pause = argc > 1 ? AS_BOOL(args[1]) : UTRUE;
  pauseAudio(channel, pause);
  return UTRUE;
}

static TypePattern argsPauseAudio[] = {
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcPauseAudio = { implPauseAudio, "pauseAudio", 0, 2, argsPauseAudio };

static ubool implSetAudioVolume(i16 argc, Value *args, Value *out) {
  size_t channel = AS_INDEX(args[0], CHANNEL_COUNT);
  u16 volume = AS_U16(args[1]);
  setAudioVolume(channel, volume);
  return UTRUE;
}

static CFunction funcSetAudioVolume = {
  implSetAudioVolume, "setAudioVolume", 2, 0, argsNumbers
};

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *windowStaticMethods[] = {
    &funcWindowStaticCall,
    NULL,
  };
  CFunction *windowMethods[] = {
    &funcWindowGetattr,
    &funcWindowMainLoop,
    &funcWindowSetTitle,
    &funcWindowSetBackgroundColor,
    &funcWindowOnUpdate,
    &funcWindowOnClick,
    &funcWindowOnClickUp,
    &funcWindowOnKeyDown,
    &funcWindowOnKeyUp,
    &funcWindowOnMotion,
    &funcWindowGetCanvas,
    &funcWindowNewCanvas,
    &funcWindowClear,
    &funcWindowNewTexture,
    &funcWindowNewPolygon,
    NULL,
  };
  CFunction *textureStaticMethods[] = {
    NULL,
  };
  CFunction *textureMethods[] = {
    &funcTextureGetattr,
    &funcTextureBlit,
    &funcTextureIsStreaming,
    &funcTextureUpdate,
    NULL,
  };
  CFunction *geometryStaticMethods[] = {
    NULL,
  };
  CFunction *geometryMethods[] = {
    &funcGeometryGetattr,
    &funcGeometryBlit,
    &funcGeometrySetColor,
    &funcGeometrySetVertexColor,
    NULL,
  };
  CFunction *clickEventStaticMethods[] = {
    NULL,
  };
  CFunction *clickEventMethods[] = {
    &funcClickEventGetattr,
    NULL,
  };
  CFunction *keyEventStaticMethods[] = {
    NULL,
  };
  CFunction *keyEventMethods[] = {
    &funcKeyEventGetattr,
    NULL,
  };
  CFunction *motionEventStaticMethods[] = {
    NULL,
  };
  CFunction *motionEventMethods[] = {
    &funcMotionEventGetattr,
    NULL,
  };
  CFunction *functions[] = {
    &funcLoadAudio,
    &funcPlayAudio,
    &funcPauseAudio,
    &funcSetAudioVolume,
    NULL,
  };
  ubool gcPause;

  ggModule = module;

  if (!importModuleAndPop("media.canvas")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.audio")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.font")) {
    return UFALSE;
  }

  if (!importModuleAndPop("media.font.roboto.mono")) {
    return UFALSE;
  }

  LOCAL_GC_PAUSE(gcPause);

  moduleRetain(module, STRING_VAL(buttonString = internCString("button")));
  moduleRetain(module, STRING_VAL(keyString = internCString("key")));
  moduleRetain(module, STRING_VAL(scancodeString = internCString("scancode")));
  moduleRetain(module, STRING_VAL(repeatString = internCString("repeat")));
  moduleRetain(module, STRING_VAL(dxString = internCString("dx")));
  moduleRetain(module, STRING_VAL(dyString = internCString("dy")));
  moduleRetain(module, STRING_VAL(transformString = internCString("transform")));

  {
    ScancodeEntry *entry = scancodeEntries;
    for (; entry->name; entry++) {
      String *name = internCString(entry->name);
      i32 scancode = entry->scancode;
      if (scancode < 0 || scancode >= SCANCODE_KEY_COUNT) {
        panic("Scancode out of bounds: %d", scancode);
      }
      moduleRetain(module, STRING_VAL(name));
      scancodeKeys[scancode] = name;
    }
  }

  moduleRetain(module, CLICK_EVENT_VAL(
    clickEvent = NEW_NATIVE(ObjClickEvent, &descriptorClickEvent)));
  moduleRetain(module, KEY_EVENT_VAL(
    keyEvent = NEW_NATIVE(ObjKeyEvent, &descriptorKeyEvent)));
  moduleRetain(module, MOTION_EVENT_VAL(
    motionEvent = NEW_NATIVE(ObjMotionEvent, &descriptorMotionEvent)));

  moduleAddFunctions(module, functions);

  newNativeClass(
    module,
    &descriptorWindow,
    windowMethods,
    windowStaticMethods);

  newNativeClass(
    module,
    &descriptorTexture,
    textureMethods,
    textureStaticMethods);

  newNativeClass(
    module,
    &descriptorGeometry,
    geometryMethods,
    geometryStaticMethods);

  newNativeClass(
    module,
    &descriptorClickEvent,
    clickEventMethods,
    clickEventStaticMethods);

  newNativeClass(
    module,
    &descriptorKeyEvent,
    keyEventMethods,
    keyEventStaticMethods);

  newNativeClass(
    module,
    &descriptorMotionEvent,
    motionEventMethods,
    motionEventStaticMethods);

  {
    ColorEntry *entry;
    Value *colors;
    size_t colorCount = 0, i;
    for (entry = colorEntries; entry->name; entry++) {
      mapSetN(&module->fields, entry->name, COLOR_VAL(entry->color));
      colorCount++;
    }
    colors = malloc(sizeof(Value) * colorCount);
    for (entry = colorEntries, i = 0; entry->name; entry++, i++) {
      colors[i] = COLOR_VAL(entry->color);
    }
    mapSetN(&module->fields, "COLORS", FROZEN_LIST_VAL(copyFrozenList(colors, colorCount)));
    free(colors);
  }

  LOCAL_GC_UNPAUSE(gcPause);

  if (!initSDL()) {
    return UFALSE;
  }

  audioMutex = SDL_CreateMutex();
  if (!audioMutex) {
    return sdlError("SDL_CreateMutex");
  }

  return UTRUE;
}

static CFunction func = { impl, "gg", 1 };

void addNativeModuleGG() {
  addNativeModule(&func);
}

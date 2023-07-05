#include "mtots_m_paco.h"

#include "mtots_vm.h"
#include "mtots_sdl.h"

#include "mtots_m_paco_sdl.h"
#include "mtots_m_paco_render.h"
#include "mtots_m_paco_ss.h"
#include "mtots_m_paco_bmp.h"
#include "mtots_m_paco_scancode.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define DEFAULT_RENDER_WIDTH         128
#define DEFAULT_RENDER_HEIGHT        128
#define PLAYER_COUNT                   8
#define BUTTON_COUNT                   8
#define PRESSED                        0
#define RELEASED                       1
#define HELD                           2
#define MOUSE_BUTTON_LEFT              0
#define MOUSE_BUTTON_RIGHT             1
#define MOUSE_BUTTON_MIDDLE            2
#define CLICK_PRESSED                  0
#define CLICK_RELEASED                 1
#define CLICK_HELD                     2
#define DEFAULT_PEN_COLOR     LIGHT_GREY
#define KEY_QUEUE_MAX                 16
#define FONT_TOFU_INDEX                0

typedef enum RetainSlot {
  RETAIN_SLOT_WINDOW_TITLE,
  RETAIN_SLOT_RENDER_TARGET,
  RETAIN_SLOT_ACTIVE_SPRITE_SHEET,
  RETAIN_SLOT_ACTIVE_FONT,
  RETAIN_SLOT_PALETTE,
  RETAIN_SLOT_COUNT
} RetainSlot;

/* BEGIN module retained values */
static ObjModule *pacoModule;
static ObjList *retainList;
static String *windowTitle;
static ObjSpriteSheet *renderTarget;
static ObjSpriteSheet *activeSpriteSheet;
static ObjSpriteSheet *activeFont;
static ObjBuffer *palette; /* Should always have length U8_COUNT */
/* BEGIN module retained values */

/* BEGIN options */
static size_t renderWidth = DEFAULT_RENDER_WIDTH;
static size_t renderHeight = DEFAULT_RENDER_HEIGHT;
static size_t fps = 60;
static double scalingFactor = 1.0;
static u32 pacoRandomSeed = 19937;
/* END options */

static ubool initialized = UFALSE;
static Random pacoRandom;
static SDL_Window *window;
static SDL_Surface *windowSurface;
static SDL_Surface *convertedSurface;
static SDL_Surface *surface;
static u8 *framebuffer;
static i32 clipXStart, clipYStart, clipXEnd, clipYEnd;
static u8 penColor = DEFAULT_PEN_COLOR;
static i32 cameraX, cameraY;
static i32 cursorX, cursorY;
static u8 paletteForPrint[U8_COUNT];
static u8 buttons[3][PLAYER_COUNT][BUTTON_COUNT];
static u8 clicks[3][3];
static i32 mouseX, mouseY;
static float wheelX, wheelY;
static int keyboardStateLength;
static const Uint8 *keyboardState;
static u8 pressedKeys[KEY_QUEUE_MAX];
static size_t pressedKeysLength, pressedKeysNext;
static u8 releasedKeys[KEY_QUEUE_MAX];
static size_t releasedKeysLength, releasedKeysNext;

static void resetClip() {
  clipXStart = clipYStart = 0;
  clipXEnd = clipYEnd = I32_MAX;
}

static void setClip(i32 xStart, i32 yStart, i32 xEnd, i32 yEnd) {
  clipXStart = xStart;
  clipYStart = yStart;
  clipXEnd = xEnd;
  clipYEnd = yEnd;
}

static void normalizeRenderTarget(RenderTarget *rt) {
  /* clip */
  rt->clipXStart = clipXStart < 0 ? 0 : clipXStart;
  rt->clipYStart = clipYStart < 0 ? 0 : clipYStart;
  rt->clipXEnd = clipXEnd > rt->width ? rt->width : clipXEnd;
  rt->clipYEnd = clipYEnd > rt->height ? rt->height : clipYEnd;

  /* palette */
  if (palette->handle.length != U8_COUNT) {
    panic("ASSERTION ERROR, UNEXPECTED PALETTE SIZE");
  }
  rt->palette = palette->handle.data;
}

static RenderTarget screenAsRenderTarget() {
  RenderTarget rt;
  rt.width = renderWidth;
  rt.height = renderHeight;
  rt.framebuffer = framebuffer;
  normalizeRenderTarget(&rt);
  return rt;
}

static RenderTarget spriteSheetAsRenderTarget(ObjSpriteSheet *sheet) {
  RenderTarget rt;
  rt.width = sheet->width;
  rt.height = sheet->height;
  rt.framebuffer = sheet->pixels;
  normalizeRenderTarget(&rt);
  return rt;
}

static RenderTarget getActiveRenderTarget() {
  return renderTarget ? spriteSheetAsRenderTarget(renderTarget) : screenAsRenderTarget();
}

static ObjSpriteSheet *getActiveSpriteSheet() {
  return activeSpriteSheet;
}

static ObjSpriteSheet *getActiveFont() {
  return activeFont;
}

static RenderTarget getActiveRenderTargetForPrint(u8 color) {
  RenderTarget rt = getActiveRenderTarget();
  color = rt.palette[color];
  paletteForPrint[WHITE] = color;
  rt.palette = paletteForPrint;
  return rt;
}

static void tickButtons() {
  memset(buttons, 0, PLAYER_COUNT * BUTTON_COUNT * 2);
}

static void checkButtonAndPlayer(u8 buttonID, u8 playerID) {
  if (buttonID >= BUTTON_COUNT) {
    panic("Invalid buttonID %d", buttonID);
  }
  if (playerID >= PLAYER_COUNT) {
    panic("Invalid playerID %d", playerID);
  }
}

static void pressButton(u8 buttonID, u8 playerID) {
  checkButtonAndPlayer(buttonID, playerID);
  buttons[PRESSED][playerID][buttonID] = UTRUE;
  buttons[HELD][playerID][buttonID] = UTRUE;
}

static void releaseButton(u8 buttonID, u8 playerID) {
  checkButtonAndPlayer(buttonID, playerID);
  buttons[RELEASED][playerID][buttonID] = UTRUE;
  buttons[HELD][playerID][buttonID] = UFALSE;
}

static ubool queryButton(u8 query, u8 buttonID, u8 playerID) {
  checkButtonAndPlayer(buttonID, playerID);
  if (query > HELD) {
    panic("Invalid button query %d", query);
  }
  return buttons[query][playerID][buttonID];
}

static void tickClicks() {
  memset(clicks, 0, 2 * 3);
}

static void checkMouseButtonID(u8 mouseButtonID) {
  if (mouseButtonID >= 3) {
    panic("Invalid mouse button ID %d", mouseButtonID);
  }
}

static void pressClick(u8 mouseButtonID) {
  checkMouseButtonID(mouseButtonID);
  clicks[CLICK_PRESSED][mouseButtonID] = UTRUE;
  clicks[CLICK_HELD][mouseButtonID] = UTRUE;
}

static void releaseClick(u8 mouseButtonID) {
  checkMouseButtonID(mouseButtonID);
  clicks[CLICK_RELEASED][mouseButtonID] = UTRUE;
  clicks[CLICK_HELD][mouseButtonID] = UFALSE;
}

static ubool queryClick(u8 query, u8 mouseButtonID) {
  checkMouseButtonID(mouseButtonID);
  if (query > CLICK_HELD) {
    panic("Invalid mouse click query %d", query);
  }
  return clicks[query][mouseButtonID];
}

static void setWindowTitle(String *newTitle) {
  retainList->buffer[RETAIN_SLOT_WINDOW_TITLE] = newTitle ? STRING_VAL(newTitle) : NIL_VAL();
  windowTitle = newTitle;
  if (initialized) {
    SDL_SetWindowTitle(window, newTitle ? newTitle->chars : "");
  }
}

static void setRenderTarget(ObjSpriteSheet *sheet) {
  retainList->buffer[RETAIN_SLOT_RENDER_TARGET] = sheet ? OBJ_VAL_EXPLICIT((Obj*)sheet) : NIL_VAL();
  renderTarget = sheet;
  resetClip();
}

static void setActiveSpriteSheet(ObjSpriteSheet *sheet) {
  retainList->buffer[RETAIN_SLOT_ACTIVE_SPRITE_SHEET] = sheet ? OBJ_VAL_EXPLICIT((Obj*)sheet) : NIL_VAL();
  activeSpriteSheet = sheet;
}

static void setActiveFont(ObjSpriteSheet *font) {
  retainList->buffer[RETAIN_SLOT_ACTIVE_FONT] = font ? OBJ_VAL_EXPLICIT((Obj*)font) : NIL_VAL();
  activeFont = font;
}

static void setPalette(ObjBuffer *newPalette) {
  if (!newPalette) {
    panic("The palette buffer cannot be null");
  }
  if (newPalette->handle.length != U8_COUNT) {
    panic(
      "Palette size must be %d but got %lu",
      U8_COUNT,
      (unsigned long)newPalette->handle.length);
  }
  bufferLock(&newPalette->handle);
  retainList->buffer[RETAIN_SLOT_PALETTE] = BUFFER_VAL(newPalette);
  palette = newPalette;
}

static ObjBuffer *newPalette() {
  ObjBuffer *palette = newBuffer();
  push(BUFFER_VAL(palette));
  bufferSetLength(&palette->handle, U8_COUNT);
  bufferLock(&palette->handle);
  pop(); /* palette */
  return palette;
}

static void initPalette() {
  size_t i;
  ObjBuffer *palette = newPalette();
  palette->handle.data[0] = U8_MAX;
  for (i = 1; i < U8_COUNT; i++) {
    palette->handle.data[i] = i;
  }
  setPalette(palette);
}

static ubool resetSurfaces() {
  windowSurface = SDL_GetWindowSurface(window);
  if (!windowSurface) {
    runtimeError("paco.init(): SDL_getWindowSurface failed: %s", SDL_GetError());
    return UFALSE;
  }

  SDL_FreeSurface(convertedSurface);
  convertedSurface = SDL_CreateRGBSurfaceWithFormat(
    0,
    renderWidth,
    renderHeight,
    SDL_BITSPERPIXEL(windowSurface->format->format),
    windowSurface->format->format);
  if (!convertedSurface) {
    runtimeError("paco.init(): SDL_CreateRGBSurfaceWithFormat failed: %s", SDL_GetError());
    return UFALSE;
  }

  SDL_FreeSurface(surface);
  free(framebuffer);
  framebuffer = calloc(renderWidth * renderHeight, 1);
  surface = SDL_CreateRGBSurfaceWithFormatFrom(
    framebuffer,
    renderWidth,
    renderHeight,
    SDL_BITSPERPIXEL(SDL_PIXELFORMAT_INDEX8),
    calculatePitch(SDL_PIXELFORMAT_INDEX8, renderWidth),
    SDL_PIXELFORMAT_INDEX8);
  if (!surface) {
    runtimeError("paco.init(): SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());
    return UFALSE;
  }
  if (SDL_SetPaletteColors(
      surface->format->palette,
      systemColors,
      0,
      sizeof(systemColors)/sizeof(SDL_Color)) != 0) {
    runtimeError("paco.init(): SDL_SetPaletteColors failed: %s", SDL_GetError());
    return UFALSE;
  }

  resetClip();
  return UTRUE;
}

static ubool resetWindow() {
  int scaledWidth = renderWidth * scalingFactor;
  int scaledHeight = renderHeight * scalingFactor;
  SDL_SetWindowSize(window, scaledWidth, scaledHeight);
  return resetSurfaces();
}

static ubool setScalingFactor(double newScalingFactor) {
  scalingFactor = newScalingFactor;
  if (initialized) {
    if (!resetWindow()) {
      return UFALSE;
    }
  }
  return UTRUE;
}

static ubool setRenderDimensions(size_t newRenderWidth, size_t newRenderHeight) {
  renderWidth = newRenderWidth;
  renderHeight = newRenderHeight;
  if (renderWidth % 4 != 0) {
    runtimeError(
      "render width must be a multiple of 4 but got %lu",
      (unsigned long)renderWidth);
    return UFALSE;
  }
  if (initialized) {
    if (!resetWindow()) {
      return UFALSE;
    }
  }
  return UTRUE;
}

static ubool implSetWindowTitle(i16 argCount, Value *args, Value *out) {
  String *newWindowTitle = AS_STRING(args[0]);
  setWindowTitle(newWindowTitle);
  return UTRUE;
}

static CFunction funcSetWindowTitle = {
  implSetWindowTitle, "setWindowTitle", 1, 0, argsStrings
};

static ubool implSetScalingFactor(i16 argCount, Value *args, Value *out) {
  double newScalingFactor = AS_NUMBER(args[0]);
  return setScalingFactor(newScalingFactor);
}

static CFunction funcSetScalingFactor = {
  implSetScalingFactor, "setScalingFactor", 1, 0, argsNumbers,
};

static ubool implSetRenderDimensions(i16 argCount, Value *args, Value *out) {
  size_t newRenderWidth = AS_SIZE(args[0]);
  size_t newRenderHeight = AS_SIZE(args[1]);
  return setRenderDimensions(newRenderWidth, newRenderHeight);
}

static CFunction funcSetRenderDimensions = {
  implSetRenderDimensions, "setRenderDimensions", 2, 0, argsNumbers,
};

static ubool implInit(i16 argCount, Value *args, Value *out) {
  double scaledWidth = renderWidth * scalingFactor;
  double scaledHeight = renderHeight * scalingFactor;
  Uint32 windowFlags = 0;

  initialized = UTRUE;

  initRandom(&pacoRandom, pacoRandomSeed);

  if (!initSDL()) {
    return UFALSE;
  }

  keyboardState = SDL_GetKeyboardState(&keyboardStateLength);

  window = SDL_CreateWindow(
    windowTitle ? windowTitle->chars : "paco",
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    scaledWidth,
    scaledHeight,
    windowFlags);
  if (!window) {
    runtimeError("paco.init(): SDL_CreateWidnow failed: %s", SDL_GetError());
    return UFALSE;
  }

  resetSurfaces();

  return UTRUE;
}

static CFunction funcInit = { implInit, "init" };

static ubool implMainLoop(i16 argCount, Value *args, Value *out) {
  SDL_Event event;
  Value update = args[0];
  Value draw = argCount > 1 ? args[1] : NIL_VAL();
  Uint64 countPerSecond = SDL_GetPerformanceFrequency();
  double millisecondsPerCount = 1000 / (double)countPerSecond;
  Uint64 countPerFrame = fps > 0 ? countPerSecond / fps : 0;
  Uint64 lastCount = SDL_GetPerformanceCounter();

  for (;;) {
    Uint64 newCount;
    tickButtons();
    tickClicks();
    wheelX = wheelY = 0;
    pressedKeysLength = releasedKeysLength = 0;
    pressedKeysNext = releasedKeysNext = 0;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          return UTRUE;
        case SDL_KEYDOWN:
          switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_LEFT: pressButton(BUTTON_LEFT, 0); break;
            case SDL_SCANCODE_RIGHT: pressButton(BUTTON_RIGHT, 0); break;
            case SDL_SCANCODE_UP: pressButton(BUTTON_UP, 0); break;
            case SDL_SCANCODE_DOWN: pressButton(BUTTON_DOWN, 0); break;
            case SDL_SCANCODE_Z: pressButton(BUTTON_O, 0); break;
            case SDL_SCANCODE_X: pressButton(BUTTON_X, 0); break;
            default: break;
          }
          if (pressedKeysLength < KEY_QUEUE_MAX) {
            pressedKeys[pressedKeysLength++] = event.key.keysym.scancode;
          }
          break;
        case SDL_KEYUP:
          switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_LEFT: releaseButton(BUTTON_LEFT, 0); break;
            case SDL_SCANCODE_RIGHT: releaseButton(BUTTON_RIGHT, 0); break;
            case SDL_SCANCODE_UP: releaseButton(BUTTON_UP, 0); break;
            case SDL_SCANCODE_DOWN: releaseButton(BUTTON_DOWN, 0); break;
            case SDL_SCANCODE_Z: releaseButton(BUTTON_O, 0); break;
            case SDL_SCANCODE_X: releaseButton(BUTTON_X, 0); break;
            default: break;
          }
          if (releasedKeysLength < KEY_QUEUE_MAX) {
            releasedKeys[releasedKeysLength++] = event.key.keysym.scancode;
          }
          break;
        case SDL_MOUSEBUTTONDOWN:
          switch (event.button.button) {
            case SDL_BUTTON_LEFT: pressClick(MOUSE_BUTTON_LEFT); break;
            case SDL_BUTTON_MIDDLE: pressClick(MOUSE_BUTTON_MIDDLE); break;
            case SDL_BUTTON_RIGHT: pressClick(MOUSE_BUTTON_RIGHT); break;
            default: break;
          }
          break;
        case SDL_MOUSEBUTTONUP:
          switch (event.button.button) {
            case SDL_BUTTON_LEFT: releaseClick(MOUSE_BUTTON_LEFT); break;
            case SDL_BUTTON_MIDDLE: releaseClick(MOUSE_BUTTON_MIDDLE); break;
            case SDL_BUTTON_RIGHT: releaseClick(MOUSE_BUTTON_RIGHT); break;
            default: break;
          }
          break;
        case SDL_MOUSEWHEEL:
          wheelX = event.wheel.preciseX;
          wheelY = event.wheel.preciseY;
          if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            wheelX *= -1;
            wheelY *= -1;
          }
          break;
        case SDL_MOUSEMOTION:
          mouseX = event.motion.x / scalingFactor;
          mouseY = event.motion.y / scalingFactor;
          break;
      }
    }

    push(update);
    if (!callFunction(0)) {
      return UFALSE;
    }
    pop(); /* update return value */

    newCount = SDL_GetPerformanceCounter();
    if (newCount - lastCount < countPerFrame) {
      if (!IS_NIL(draw)) {
        push(draw);
        if (!callFunction(0)) {
          return UFALSE;
        }
        pop(); /* draw return value */
      }
      if (SDL_BlitSurface(surface, NULL, convertedSurface, NULL) != 0) {
        runtimeError("paco.mainLoop(): SDL_BlitSurface failed: %s", SDL_GetError());
        return UFALSE;
      }
      SDL_FillRect(windowSurface, NULL, 0);
      if (SDL_BlitScaled(
          convertedSurface,
          NULL,
          windowSurface,
          NULL) != 0) {
        runtimeError("paco.mainLoop(): SDL_BlitScaled failed: %s", SDL_GetError());
        return UFALSE;
      }
      SDL_UpdateWindowSurface(window);
      newCount = SDL_GetPerformanceCounter();
    }

    if (newCount - lastCount < countPerFrame) {
      Uint32 ms = millisecondsPerCount * (countPerFrame - (newCount - lastCount));
#ifdef __EMSCRIPTEN__
      emscripten_sleep(ms);
#else
      SDL_Delay(ms);
#endif
    }
    lastCount = newCount;
  }
  return UTRUE;
}

static CFunction funcMainLoop = { implMainLoop, "mainLoop", 1, 2 };

static ubool implClear(i16 argCount, Value *args, Value *out) {
  u8 color = AS_U8(args[0]);
  memset(framebuffer, color, renderWidth * renderHeight);
  return UTRUE;
}

static CFunction funcClear = { implClear, "clear", 1, 0, argsNumbers };

static ubool implNewSheet(i16 argCount, Value *args, Value *out) {
  size_t width = AS_SIZE(args[0]);
  size_t height = AS_SIZE(args[1]);
  size_t spriteWidth = AS_SIZE(args[2]);
  size_t spriteHeight = AS_SIZE(args[3]);
  ObjSpriteSheet *sheet;
  if (!newSpriteSheet(width, height, spriteWidth, spriteHeight, &sheet)) {
    return UFALSE;
  }
  *out = OBJ_VAL_EXPLICIT((Obj*)sheet);
  return UTRUE;
}

static CFunction funcNewSheet = {
  implNewSheet, "newSheet", 4, 0, argsNumbers
};

static ubool implSaveSheetToFile(i16 argCount, Value *args, Value *out) {
  ObjSpriteSheet *sheet = getActiveSpriteSheet();
  String *fileName = AS_STRING(args[0]);
  if (!sheet) {
    runtimeError("There is no active sprite sheet to save to %s", fileName->chars);
    return UFALSE;
  }
  return saveSpriteSheetToFile(sheet, fileName->chars);
}

static CFunction funcSaveSheetToFile = {
  implSaveSheetToFile, "saveSheetToFile", 1, 0, argsStrings
};

static ubool implLoadSheetFromFile(i16 argCount, Value *args, Value *out) {
  String *fileName = AS_STRING(args[0]);
  size_t spriteWidth = argCount > 1 ? AS_SIZE(args[1]) : DEFAULT_SPRITE_WIDTH;
  size_t spriteHeight = argCount > 2 ? AS_SIZE(args[2]) : DEFAULT_SPRITE_HEIGHT;
  ubool setActive = argCount > 3 ? AS_SIZE(args[3]) : UTRUE;
  ObjSpriteSheet *sheet;
  if (!loadSpriteSheetFromFile(fileName->chars, spriteWidth, spriteHeight, &sheet)) {
    return UFALSE;
  }
  if (setActive) {
    setActiveSpriteSheet(sheet);
  }
  *out = OBJ_VAL_EXPLICIT((Obj*)sheet);
  return UTRUE;
}

static TypePattern argsLoadSheetFromFile[] = {
  { TYPE_PATTERN_STRING },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcLoadSheetFromFile = {
  implLoadSheetFromFile, "loadSheetFromFile", 1, 4, argsLoadSheetFromFile,
};

static ubool implLoadSheetFromBuffer(i16 argCount, Value *args, Value *out) {
  ObjBuffer *buffer = AS_BUFFER(args[0]);
  size_t spriteWidth = argCount > 1 ? AS_SIZE(args[1]) : DEFAULT_SPRITE_WIDTH;
  size_t spriteHeight = argCount > 2 ? AS_SIZE(args[2]) : DEFAULT_SPRITE_HEIGHT;
  ubool setActive = argCount > 3 ? AS_SIZE(args[3]) : UTRUE;
  ObjSpriteSheet *sheet;
  if (!loadSpriteSheetFromBuffer(&buffer->handle, spriteWidth, spriteHeight, &sheet)) {
    return UFALSE;
  }
  if (setActive) {
    setActiveSpriteSheet(sheet);
  }
  *out = OBJ_VAL_EXPLICIT((Obj*)sheet);
  return UTRUE;
}

static TypePattern argsLoadSheetFromBuffer[] = {
  { TYPE_PATTERN_BUFFER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_NUMBER },
  { TYPE_PATTERN_BOOL },
};

static CFunction funcLoadSheetFromBuffer = {
  implLoadSheetFromBuffer, "loadSheetFromBuffer", 1, 4, argsLoadSheetFromBuffer,
};

static ubool implRenderTarget(i16 argCount, Value *args, Value *out) {
  setRenderTarget(argCount <= 0 || IS_NIL(args[0]) ? NULL : (ObjSpriteSheet*)AS_OBJ(args[0]));
  return UTRUE;
}

static TypePattern argsRenderTarget[] = {
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorSpriteSheet },
};

static CFunction funcRenderTarget = {
  implRenderTarget, "renderTarget", 0, 1, argsRenderTarget
};

static ubool implSheet(i16 argCount, Value *args, Value *out) {
  setActiveSpriteSheet(IS_NIL(args[0]) ? NULL : (ObjSpriteSheet*)AS_OBJ(args[0]));
  return UTRUE;
}

static TypePattern argsSheet[] = {
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorSpriteSheet },
};

static CFunction funcSheet = { implSheet, "sheet", 1, 0, argsSheet };

static ubool implFont(i16 argCount, Value *args, Value *out) {
  setActiveFont(IS_NIL(args[0]) ? NULL : (ObjSpriteSheet*)AS_OBJ(args[0]));
  return UTRUE;
}

static TypePattern argsFont[] = {
  { TYPE_PATTERN_NATIVE_OR_NIL, &descriptorSpriteSheet },
};

static CFunction funcFont = { implFont, "font", 1, 0, argsFont };

static ubool implNewPalette(i16 argCount, Value *args, Value *out) {
  ObjBuffer *buffer = newPalette();
  memcpy(buffer->handle.data, palette->handle.data, U8_COUNT);
  *out = BUFFER_VAL(buffer);
  return UTRUE;
}

static CFunction funcNewPalette = { implNewPalette, "newPalette" };

static ubool implPalette(i16 argCount, Value *args, Value *out) {
  if (argCount > 0 && !IS_NIL(args[0])) {
    setPalette(AS_BUFFER(args[0]));
  }
  *out = BUFFER_VAL(palette);
  return UTRUE;
}

static TypePattern argsPalette[] = { { TYPE_PATTERN_BUFFER_OR_NIL } };

static CFunction funcPalette = { implPalette, "palette", 0, 1, argsPalette };

static ubool implColor(i16 argCount, Value *args, Value *out) {
  penColor = AS_U8(args[0]);
  return UTRUE;
}

static CFunction funcColor = { implColor, "color", 1, 0, argsNumbers };

static ubool implClip(i16 argCount, Value *args, Value *out) {
  switch (argCount) {
    case 0:
      resetClip();
      return UTRUE;
    case 4:
      setClip(AS_I32(args[0]), AS_I32(args[1]), AS_I32(args[2]), AS_I32(args[3]));
      return UTRUE;
    default:
      runtimeError("paco.clip() requires exactly 0 or 4 arguments but got %d", argCount);
      return UFALSE;
  }
}

static CFunction funcClip = { implClip, "clip", 0, 4, argsNumbers };

static ubool implCamera(i16 argCount, Value *args, Value *out) {
  cameraX = AS_I32(args[0]);
  cameraY = AS_I32(args[1]);
  return UTRUE;
}

static CFunction funcCamera = { implCamera, "camera", 2, 0, argsNumbers };

static ubool implPixel(i16 argCount, Value *args, Value *out) {
  i32 x = AS_I32(args[0]);
  i32 y = AS_I32(args[1]);
  i32 color = argCount > 2 && AS_I32(args[2]) >= 0 ? AS_U8(args[2]) : -1;
  RenderTarget rt = getActiveRenderTarget();
  x -= cameraX;
  y -= cameraY;
  if (color != -1) {
    setPixel(&rt, x, y, (u8)color);
  }
  *out = NUMBER_VAL(getPixel(&rt, x, y));
  return UTRUE;
}

static CFunction funcPixel = { implPixel, "pixel", 2, 3, argsNumbers };

static ubool implLine(i16 argCount, Value *args, Value *out) {
  i32 x0 = AS_I32(args[0]);
  i32 y0 = AS_I32(args[1]);
  i32 x1 = AS_I32(args[2]);
  i32 y1 = AS_I32(args[3]);
  u8 style = argCount > 4 && AS_I32(args[4]) >= 0 ? AS_U8(args[4]) : DRAW_STYLE_OUTLINE;
  u8 color = argCount > 5 && AS_I32(args[5]) >= 0 ? AS_U8(args[5]) : penColor;
  RenderTarget rt = getActiveRenderTarget();
  x0 -= cameraX;
  y0 -= cameraY;
  x1 -= cameraX;
  y1 -= cameraY;
  return drawLine(&rt, x0, y0, x1, y1, style, color);
}

static CFunction funcLine = { implLine, "line", 4, 6, argsNumbers };

static ubool implRect(i16 argCount, Value *args, Value *out) {
  i32 x0 = AS_I32(args[0]);
  i32 y0 = AS_I32(args[1]);
  i32 x1 = AS_I32(args[2]);
  i32 y1 = AS_I32(args[3]);
  u8 style = argCount > 4 && AS_I32(args[4]) >= 0 ? AS_U8(args[4]) : DRAW_STYLE_FILL;
  u8 color = argCount > 5 && AS_I32(args[5]) >= 0 ? AS_U8(args[5]) : penColor;
  RenderTarget rt = getActiveRenderTarget();
  x0 -= cameraX;
  y0 -= cameraY;
  x1 -= cameraX;
  y1 -= cameraY;
  return drawRect(&rt, x0, y0, x1, y1, style, color);
}

static CFunction funcRect = { implRect, "rect", 4, 6, argsNumbers };

static ubool implOval(i16 argCount, Value *args, Value *out) {
  i32 x0 = AS_I32(args[0]);
  i32 y0 = AS_I32(args[1]);
  i32 x1 = AS_I32(args[2]);
  i32 y1 = AS_I32(args[3]);
  u8 style = argCount > 4 && AS_I32(args[4]) >= 0 ? AS_U8(args[4]) : DRAW_STYLE_FILL;
  u8 color = argCount > 5 && AS_I32(args[5]) >= 0 ? AS_U8(args[5]) : penColor;
  i32 cx, cy, a, b;
  RenderTarget rt = getActiveRenderTarget();
  x0 -= cameraX;
  y0 -= cameraY;
  x1 -= cameraX;
  y1 -= cameraY;
  cx = (x0 + x1) / 2;
  cy = (y0 + y1) / 2;
  a = iabs(x1 - x0) / 2;
  b = iabs(y1 - y0) / 2;
  return drawEllipse(&rt, cx, cy, a, b, style, color);
}

static CFunction funcOval = { implOval, "oval", 4, 6, argsNumbers };

static ubool implSprite(i16 argCount, Value *args, Value *out) {
  i32 n = AS_I32(args[0]);
  i32 x = AS_I32(args[1]);
  i32 y = AS_I32(args[2]);
  i32 w = argCount > 3 ? AS_I32(args[3]) : 1;
  i32 h = argCount > 4 ? AS_I32(args[4]) : 1;
  i32 flipX = argCount > 5 ? AS_I32(args[5]) : 0;
  i32 flipY = argCount > 6 ? AS_I32(args[6]) : 0;
  ObjSpriteSheet *sheet = getActiveSpriteSheet();
  RenderTarget rt = getActiveRenderTarget();
  if (!sheet) {
    runtimeError("paco.sheet(): No active sprite sheet");
    return UFALSE;
  }
  x -= cameraX;
  y -= cameraY;
  return drawSprite(&rt, sheet, n, x, y, w, h, !!flipX, !!flipY);
}

static CFunction funcSprite = { implSprite, "sprite", 3, 7, argsNumbers };

static ubool implBlit(i16 argCount, Value *args, Value *out) {
  i32 sx = AS_I32(args[0]);
  i32 sy = AS_I32(args[1]);
  i32 sw = AS_I32(args[2]);
  i32 sh = AS_I32(args[3]);
  i32 dx = AS_I32(args[4]);
  i32 dy = AS_I32(args[5]);
  i32 dw = argCount > 6 ? AS_I32(args[6]) : -1;
  i32 dh = argCount > 7 ? AS_I32(args[7]) : -1;
  i32 flipX = argCount > 8 ? AS_I32(args[8]) : 0;
  i32 flipY = argCount > 9 ? AS_I32(args[9]) : 0;
  ObjSpriteSheet *sheet = getActiveSpriteSheet();
  RenderTarget rt = getActiveRenderTarget();
  if (!sheet) {
    runtimeError("paco.sheet(): No active sprite sheet");
    return UFALSE;
  }
  dx -= cameraX;
  dy -= cameraY;
  dw = dw == -1 ? sw : dw;
  dh = dh == -1 ? sh : dh;
  return drawSheet(&rt, sheet, sx, sy, sw, sh, dx, dy, dw, dh, !!flipX, !!flipY);
}

static CFunction funcBlit = { implBlit, "blit", 6, 10, argsNumbers };

static ubool implCursor(i16 argCount, Value *args, Value *out) {
  cursorX = AS_I32(args[0]);
  cursorY = AS_I32(args[1]);
  return UTRUE;
}

static CFunction funcCursor = { implCursor, "cursor", 2, 0, argsNumbers };

static ubool implPrint(i16 argCount, Value *args, Value *out) {
  String *text = AS_STRING(args[0]);
  u8 color = penColor;
  i32 nextX;
  size_t i;
  ObjSpriteSheet *font = getActiveFont();
  if (argCount > 1 && !IS_NIL(args[1])) {
    cursorX = AS_I32(args[1]);
  }
  if (argCount > 2 && !IS_NIL(args[2])) {
    cursorY = AS_I32(args[2]);
  }
  if (argCount > 3 && !IS_NIL(args[3])) {
    color = AS_U8(args[3]);
  }
  if (!font) {
    runtimeError("There is no active font to use with print");
    return UFALSE;
  }
  nextX = cursorX;
  {
    RenderTarget rt = getActiveRenderTargetForPrint(color);
    for (i = 0; i < text->codePointCount; i++) {
      size_t fontLimit = (font->width / font->spriteWidth) * (font->height / font->spriteHeight);
      u32 ch = text->utf32 ? (u32)text->utf32[i] : (u32)text->chars[i];
      if (ch == '\n') {
        nextX = cursorX;
        cursorY += font->spriteHeight;
      } else {
        if (ch >= fontLimit) {
          ch = FONT_TOFU_INDEX;
        }
        drawSprite(
          &rt,
          font,
          (i32)ch,
          nextX, cursorY,
          1, 1,
          UFALSE, UFALSE);
        nextX += font->spriteWidth;
      }
    }
  }
  cursorY += font->spriteHeight;
  return UTRUE;
}

static TypePattern argsPrint[] = {
  { TYPE_PATTERN_STRING },
  { TYPE_PATTERN_NUMBER_OR_NIL },
  { TYPE_PATTERN_NUMBER_OR_NIL },
  { TYPE_PATTERN_NUMBER_OR_NIL },
};

static CFunction funcPrint = { implPrint, "print", 1, 4, argsPrint };

static ubool implButton(i16 argCount, Value *args, Value *out) {
  u8 buttonID = AS_U8(args[0]);
  u8 playerID = argCount > 1 ? AS_U8(args[1]) : 0;
  u8 query = argCount > 2 ? AS_U8(args[2]) : PRESSED;
  *out = BOOL_VAL(queryButton(query, buttonID, playerID));
  return UTRUE;
}

static CFunction funcButton = { implButton, "button", 1, 3, argsNumbers };

static ubool implClick(i16 argCount, Value *args, Value *out) {
  u8 mouseButtonID = argCount > 0 ? AS_U8(args[0]) : MOUSE_BUTTON_LEFT;
  u8 query = argCount > 1 ? AS_U8(args[1]) : CLICK_PRESSED;
  *out = BOOL_VAL(queryClick(query, mouseButtonID));
  return UTRUE;
}

static CFunction funcClick = { implClick, "click", 0, 2, argsNumbers };

static ubool implMouseX(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(mouseX);
  return UTRUE;
}

static CFunction funcMouseX = { implMouseX, "mouseX" };

static ubool implMouseY(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(mouseY);
  return UTRUE;
}

static CFunction funcMouseY = { implMouseY, "mouseY" };

static ubool implWheelX(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(wheelX);
  return UTRUE;
}

static CFunction funcWheelX = { implWheelX, "wheelX" };

static ubool implWheelY(i16 argCount, Value *args, Value *out) {
  *out = NUMBER_VAL(wheelY);
  return UTRUE;
}

static CFunction funcWheelY = { implWheelY, "wheelY" };

static ubool implKey(i16 argCount, Value *args, Value *out) {
  i32 scancode = argCount > 0 ? AS_I32(args[0]) : -1;
  u8 query = argCount > 1 ? AS_U8(args[1]) : HELD;
  if (scancode == -1) {
    switch (query) {
      case HELD:
      case PRESSED:
        *out = NUMBER_VAL(
          pressedKeysNext < pressedKeysLength ?
            pressedKeys[pressedKeysNext++] : -1);
        break;
      case RELEASED:
        *out = NUMBER_VAL(
          releasedKeysNext < releasedKeysLength ?
            releasedKeys[releasedKeysNext++] : -1);
        break;
      default:
        runtimeError("Invalid key pop query %d", query);
        return UFALSE;
    }
  } else {
    switch (query) {
      case PRESSED: {
        size_t i;
        for (i = 0; i < pressedKeysLength; i++) {
          if (pressedKeys[i] == scancode) {
            *out = NUMBER_VAL(1);
            return UTRUE;
          }
        }
        NUMBER_VAL(0);
        break;
      }
      case RELEASED: {
        size_t i;
        for (i = 0; i < releasedKeysLength; i++) {
          if (releasedKeys[i] == scancode) {
            *out = NUMBER_VAL(1);
            return UTRUE;
          }
        }
        NUMBER_VAL(0);
        break;
      }
      case HELD:
        *out = NUMBER_VAL(
          (scancode >= 0 &&
            scancode < (i32)keyboardStateLength &&
            keyboardState[scancode]) ? 1 : 0);
        break;
      default:
        runtimeError("Invalid key query %d", query);
        return UFALSE;
    }
  }
  return UTRUE;
}

static CFunction funcKey = { implKey, "key", 0, 2, argsNumbers };

static ubool implClipboard(i16 argCount, Value *args, Value *out) {
  if (argCount > 0 && !IS_NIL(args[0])) {
    String *text = AS_STRING(args[0]);
    if (SDL_SetClipboardText(AS_STRING(args[0])->chars) < 0) {
      runtimeError("SDL_SetClipboardText failed: %s", SDL_GetError());
      return UFALSE;
    }
    *out = STRING_VAL(text);
  } else {
    char *clipboardText = SDL_GetClipboardText();
    *out = STRING_VAL(internCString(clipboardText));
    SDL_free(clipboardText);
  }
  return UTRUE;
}

static TypePattern argsClipboard[] = {
  { TYPE_PATTERN_STRING_OR_NIL },
};

static CFunction funcClipboard = { implClipboard, "clipboard", 0, 1, argsClipboard };

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = pacoModule = AS_MODULE(args[0]);
  CFunction *functions[] = {
    /* options */
    &funcSetWindowTitle,
    &funcSetScalingFactor,
    &funcSetRenderDimensions,

    /* init and main loop */
    &funcInit,
    &funcMainLoop,

    /* graphics */
    &funcClear,
    &funcNewSheet,
    &funcSaveSheetToFile,
    &funcLoadSheetFromFile,
    &funcLoadSheetFromBuffer,
    &funcRenderTarget,
    &funcSheet,
    &funcFont,
    &funcNewPalette,
    &funcPalette,
    &funcColor,
    &funcClip,
    &funcCamera,
    &funcPixel,
    &funcLine,
    &funcRect,
    &funcOval,
    &funcSprite,
    &funcBlit,
    &funcCursor,
    &funcPrint,

    /* input */
    &funcButton,
    &funcClick,
    &funcMouseX,
    &funcMouseY,
    &funcWheelX,
    &funcWheelY,
    &funcKey,
    &funcClipboard,
    NULL,
  }, **function;
  ObjDict *dict;
  size_t i;

  moduleRetain(module, LIST_VAL(retainList = newList(RETAIN_SLOT_COUNT)));

  memset(paletteForPrint, TRANSPARENT, U8_COUNT);
  initPalette();
  initSpriteSheetClass(module);
  addColorConstants(module);
  mapSetN(&module->fields, "FILL", NUMBER_VAL(DRAW_STYLE_FILL));
  mapSetN(&module->fields, "OUTLINE", NUMBER_VAL(DRAW_STYLE_OUTLINE));
  mapSetN(&module->fields, "PRESSED", NUMBER_VAL(PRESSED));
  mapSetN(&module->fields, "RELEASED", NUMBER_VAL(RELEASED));
  mapSetN(&module->fields, "HELD", NUMBER_VAL(HELD));

  mapSetN(&module->fields, "LEFT", NUMBER_VAL(BUTTON_LEFT));      /* doubles as MOUSE_BUTTON_LEFT  */
  mapSetN(&module->fields, "RIGHT", NUMBER_VAL(BUTTON_RIGHT));    /* doubles as MOUSE_BUTTON_RIGHT */
  mapSetN(&module->fields, "UP", NUMBER_VAL(BUTTON_UP));
  mapSetN(&module->fields, "DOWN", NUMBER_VAL(BUTTON_DOWN));
  mapSetN(&module->fields, "O", NUMBER_VAL(BUTTON_O));
  mapSetN(&module->fields, "X", NUMBER_VAL(BUTTON_X));
  mapSetN(&module->fields, "MIDDLE", NUMBER_VAL(MOUSE_BUTTON_MIDDLE));

  for (function = functions; *function; function++) {
    mapSetN(&module->fields, (*function)->name, CFUNCTION_VAL(*function));
  }

  dict = newDict();
  mapSetN(&module->fields, "scancode", DICT_VAL(dict));
  for (i = 0; scancodeEntries[i].name; i++) {
    mapSetN(&dict->map, scancodeEntries[i].name, NUMBER_VAL(scancodeEntries[i].scancode));
  }

  return UTRUE;
}

static CFunction func = { impl, "paco", 1 };

void addNativeModulePaco() {
  addNativeModule(&func);
}

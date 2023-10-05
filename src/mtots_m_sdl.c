#include "mtots_m_sdl.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_ENABLE_SDL
#include <SDL2/SDL.h>

#define isPoint(value) (getNativeObjectDescriptor(value) == &descriptorPoint)
#define isFPoint(value) (getNativeObjectDescriptor(value) == &descriptorFPoint)
#define isRect(value) (getNativeObjectDescriptor(value) == &descriptorRect)
#define isFRect(value) (getNativeObjectDescriptor(value) == &descriptorFRect)
#define isColor(value) (getNativeObjectDescriptor(value) == &descriptorColor)
#define isVertex(value) (getNativeObjectDescriptor(value) == &descriptorVertex)
#define isEvent(value) (getNativeObjectDescriptor(value) == &descriptorEvent)
#define isWindow(value) (getNativeObjectDescriptor(value) == &descriptorWindow)

/*
 * ███████ ████████ ██████  ██ ███    ██  ██████  ███████
 * ██         ██    ██   ██ ██ ████   ██ ██       ██
 * ███████    ██    ██████  ██ ██ ██  ██ ██   ███ ███████
 *      ██    ██    ██   ██ ██ ██  ██ ██ ██    ██      ██
 * ███████    ██    ██   ██ ██ ██   ████  ██████  ███████
 */

static String *positionString;
static String *colorString;
static String *texCoordString;

/*
 * ███████ ████████ ██████  ██    ██  ██████ ████████ ███████
 * ██         ██    ██   ██ ██    ██ ██         ██    ██
 * ███████    ██    ██████  ██    ██ ██         ██    ███████
 *      ██    ██    ██   ██ ██    ██ ██         ██         ██
 * ███████    ██    ██   ██  ██████   ██████    ██    ███████
 */

typedef struct ObjPoint {
  ObjNative obj;
  SDL_Point *handle;
  Obj *owner;     /* ObjVertex or some other object that owns handle */
  SDL_Point data; /* Only used if owner == NULL */
} ObjPoint;

typedef struct ObjFPoint {
  ObjNative obj;
  SDL_FPoint *handle;
  Obj *owner;      /* ObjVertex or some other object that owns handle */
  SDL_FPoint data; /* Only used if owner == NULL */
} ObjFPoint;

typedef struct ObjRect {
  ObjNative obj;
  SDL_Rect handle;
} ObjRect;

typedef struct ObjFRect {
  ObjNative obj;
  SDL_FRect handle;
} ObjFRect;

typedef struct ObjColor {
  ObjNative obj;
  SDL_Color *handle;
  Obj *owner;     /* object that owns handle */
  SDL_Color data; /* Only used if owner == NULL */
} ObjColor;

typedef struct ObjVertex {
  ObjNative obj;
  SDL_Vertex *handle;
  Obj *owner;      /* object that owns handle */
  SDL_Vertex data; /* Only used if owner == NULL */

  ObjFPoint *position;
  ObjColor *color;
  ObjFPoint *texCoord;
} ObjVertex;

typedef struct ObjEvent {
  ObjNative obj;
  SDL_Event handle;
} ObjEvent;

typedef struct ObjWindow {
  ObjNative obj;
  SDL_Window *handle;
  SDL_Renderer *renderer;
} ObjWindow;

static Status sdlError(const char *functionName) {
  runtimeError("%s: %s", functionName, SDL_GetError());
  return STATUS_ERROR;
}

/*
 * ██████  ██       █████   ██████ ██   ██ ███████ ███    ██
 * ██   ██ ██      ██   ██ ██      ██  ██  ██      ████   ██
 * ██████  ██      ███████ ██      █████   █████   ██ ██  ██
 * ██   ██ ██      ██   ██ ██      ██  ██  ██      ██  ██ ██
 * ██████  ███████ ██   ██  ██████ ██   ██ ███████ ██   ████
 */

static void blackenPoint(ObjNative *n) {
  ObjPoint *point = (ObjPoint *)n;
  markObject(point->owner);
}

static void blackenFPoint(ObjNative *n) {
  ObjFPoint *fpoint = (ObjFPoint *)n;
  markObject(fpoint->owner);
}

static void blackenColor(ObjNative *n) {
  ObjColor *color = (ObjColor *)n;
  markObject(color->owner);
}

static void blackenVertex(ObjNative *n) {
  ObjVertex *vertex = (ObjVertex *)n;
  markObject(vertex->owner);
  markObject((Obj *)vertex->position);
  markObject((Obj *)vertex->color);
  markObject((Obj *)vertex->texCoord);
}

/*
 * ███████ ██████  ███████ ███████
 * ██      ██   ██ ██      ██
 * █████   ██████  █████   █████
 * ██      ██   ██ ██      ██
 * ██      ██   ██ ███████ ███████
 */

static void freeWindow(ObjNative *n) {
  ObjWindow *window = (ObjWindow *)n;
  if (window->handle) {
    SDL_DestroyWindow(window->handle);
  }
}

/*
 * ██████  ███████ ███████  ██████ ██████  ██ ██████  ████████  ██████  ██████  ███████
 * ██   ██ ██      ██      ██      ██   ██ ██ ██   ██    ██    ██    ██ ██   ██ ██
 * ██   ██ █████   ███████ ██      ██████  ██ ██████     ██    ██    ██ ██████  ███████
 * ██   ██ ██           ██ ██      ██   ██ ██ ██         ██    ██    ██ ██   ██      ██
 * ██████  ███████ ███████  ██████ ██   ██ ██ ██         ██     ██████  ██   ██ ███████
 */

static NativeObjectDescriptor descriptorPoint = {
    blackenPoint,
    nopFree,
    sizeof(ObjPoint),
    "Point",
};

static NativeObjectDescriptor descriptorFPoint = {
    blackenFPoint,
    nopFree,
    sizeof(ObjFPoint),
    "FPoint",
};

static NativeObjectDescriptor descriptorRect = {
    nopBlacken,
    nopFree,
    sizeof(ObjRect),
    "Rect",
};

static NativeObjectDescriptor descriptorFRect = {
    nopBlacken,
    nopFree,
    sizeof(ObjFRect),
    "FRect",
};

static NativeObjectDescriptor descriptorColor = {
    blackenColor,
    nopFree,
    sizeof(ObjColor),
    "Color",
};

static NativeObjectDescriptor descriptorVertex = {
    blackenVertex,
    nopFree,
    sizeof(ObjVertex),
    "Vertex",
};

static NativeObjectDescriptor descriptorEvent = {
    nopBlacken,
    nopFree,
    sizeof(ObjEvent),
    "Event",
};

static NativeObjectDescriptor descriptorWindow = {
    nopBlacken,
    freeWindow,
    sizeof(ObjWindow),
    "Window",
};

/*
 * ██    ██  █████  ██      ███████ ██    ██ ███    ██  ██████ ███████
 * ██    ██ ██   ██ ██      ██      ██    ██ ████   ██ ██      ██
 * ██    ██ ███████ ██      █████   ██    ██ ██ ██  ██ ██      ███████
 *  ██  ██  ██   ██ ██      ██      ██    ██ ██  ██ ██ ██           ██
 *   ████   ██   ██ ███████ ██       ██████  ██   ████  ██████ ███████
 */

static Value valPoint(ObjPoint *point) {
  return valObjExplicit((Obj *)point);
}

static Value valFpoint(ObjFPoint *fpoint) {
  return valObjExplicit((Obj *)fpoint);
}

static Value valRect(ObjRect *rect) {
  return valObjExplicit((Obj *)rect);
}

static Value valFRect(ObjFRect *frect) {
  return valObjExplicit((Obj *)frect);
}

static Value valColor(ObjColor *color) {
  return valObjExplicit((Obj *)color);
}

static Value valVertex(ObjVertex *vertex) {
  return valObjExplicit((Obj *)vertex);
}

static Value valEvent(ObjEvent *event) {
  return valObjExplicit((Obj *)event);
}

static Value valWindow(ObjWindow *window) {
  return valObjExplicit((Obj *)window);
}

/*
 *  █████  ███████     ███████ ██    ██ ███    ██  ██████ ███████
 * ██   ██ ██          ██      ██    ██ ████   ██ ██      ██
 * ███████ ███████     █████   ██    ██ ██ ██  ██ ██      ███████
 * ██   ██      ██     ██      ██    ██ ██  ██ ██ ██           ██
 * ██   ██ ███████     ██       ██████  ██   ████  ██████ ███████
 */

static ObjPoint *asPoint(Value value) {
  if (!isPoint(value)) {
    panic("Expected Point but got %s", getKindName(value));
  }
  return (ObjPoint *)AS_OBJ_UNSAFE(value);
}

static ObjFPoint *asFPoint(Value value) {
  if (!isFPoint(value)) {
    panic("Expected FPoint but got %s", getKindName(value));
  }
  return (ObjFPoint *)AS_OBJ_UNSAFE(value);
}

static ObjRect *asRect(Value value) {
  if (!isRect(value)) {
    panic("Expected Rect but got %s", getKindName(value));
  }
  return (ObjRect *)AS_OBJ_UNSAFE(value);
}

static ObjFRect *asFRect(Value value) {
  if (!isFRect(value)) {
    panic("Expected FRect but got %s", getKindName(value));
  }
  return (ObjFRect *)AS_OBJ_UNSAFE(value);
}

static ObjColor *asColor(Value value) {
  if (!isColor(value)) {
    panic("Expected Color but got %s", getKindName(value));
  }
  return (ObjColor *)AS_OBJ_UNSAFE(value);
}

static ObjVertex *asVertex(Value value) {
  if (!isVertex(value)) {
    panic("Expected Vertex but got %s", getKindName(value));
  }
  return (ObjVertex *)AS_OBJ_UNSAFE(value);
}

static ObjEvent *asEvent(Value value) {
  if (!isEvent(value)) {
    panic("Expected Event but got %s", getKindName(value));
  }
  return (ObjEvent *)AS_OBJ_UNSAFE(value);
}

static ObjWindow *asWindow(Value value) {
  if (!isWindow(value)) {
    panic("Expected Window but got %s", getKindName(value));
  }
  return (ObjWindow *)AS_OBJ_UNSAFE(value);
}

/*
 *  ██████ ████████  ██████  ██████  ███████
 * ██         ██    ██    ██ ██   ██ ██
 * ██         ██    ██    ██ ██████  ███████
 * ██         ██    ██    ██ ██   ██      ██
 *  ██████    ██     ██████  ██   ██ ███████
 */

static ObjPoint *newPoint(SDL_Point *handle, Obj *owner) {
  ObjPoint *point = NEW_NATIVE(ObjPoint, &descriptorPoint);
  point->handle = handle ? handle : &point->data;
  point->owner = owner ? owner : NULL;
  return point;
}

static ObjFPoint *newFPoint(SDL_FPoint *handle, Obj *owner) {
  ObjFPoint *fpoint = NEW_NATIVE(ObjFPoint, &descriptorFPoint);
  fpoint->handle = handle ? handle : &fpoint->data;
  fpoint->owner = owner ? owner : NULL;
  return fpoint;
}

static ObjRect *newRect() {
  ObjRect *rect = NEW_NATIVE(ObjRect, &descriptorRect);
  return rect;
}

static ObjFRect *newFRect() {
  ObjFRect *frect = NEW_NATIVE(ObjFRect, &descriptorFRect);
  return frect;
}

static ObjColor *newColor(SDL_Color *handle, Obj *owner) {
  ObjColor *color = NEW_NATIVE(ObjColor, &descriptorColor);
  color->handle = handle ? handle : &color->data;
  color->owner = owner ? owner : NULL;
  return color;
}

static ObjVertex *newVertex(SDL_Vertex *handle, Obj *owner) {
  ObjVertex *vertex = NEW_NATIVE(ObjVertex, &descriptorVertex);
  vertex->handle = handle ? handle : &vertex->data;
  vertex->owner = owner ? owner : NULL;
  {
    ubool gcPause;
    LOCAL_GC_PAUSE(gcPause);
    vertex->position = newFPoint(&vertex->handle->position, (Obj *)vertex);
    vertex->color = newColor(&vertex->handle->color, (Obj *)vertex);
    vertex->texCoord = newFPoint(&vertex->handle->tex_coord, (Obj *)vertex);
    LOCAL_GC_UNPAUSE(gcPause);
  }
  return vertex;
}

static Status newWindow(const char *title, int width, int height, ObjWindow **out) {
  ObjWindow *window = NEW_NATIVE(ObjWindow, &descriptorWindow);
  if (SDL_CreateWindowAndRenderer(width, height, 0, &window->handle, &window->renderer) != 0) {
    return sdlError("SDL_CreateWindowAndRenderer");
  }
  *out = window;
  return STATUS_OK;
}

/*
 * ███████ ██    ██ ███    ██  ██████ ████████ ██  ██████  ███    ██ ███████
 * ██      ██    ██ ████   ██ ██         ██    ██ ██    ██ ████   ██ ██
 * █████   ██    ██ ██ ██  ██ ██         ██    ██ ██    ██ ██ ██  ██ ███████
 * ██      ██    ██ ██  ██ ██ ██         ██    ██ ██    ██ ██  ██ ██      ██
 * ██       ██████  ██   ████  ██████    ██    ██  ██████  ██   ████ ███████
 */

static Status implPollEvent(i16 argc, Value *argv, Value *out) {
  *out = valBool(SDL_PollEvent(isNil(argv[0]) ? NULL : &asEvent(argv[0])->handle));
  return STATUS_OK;
}

static CFunction funcPollEvent = {implPollEvent, "pollEvent", 1};

static Status implDelay(i16 argc, Value *argv, Value *out) {
  SDL_Delay(asU32(argv[0]));
  return STATUS_OK;
}

static CFunction funcDelay = {implDelay, "delay", 1};

static Status implGetPerformanceCounter(i16 argc, Value *argv, Value *out) {
  *out = valNumber(SDL_GetPerformanceCounter());
  return STATUS_OK;
}

static CFunction funcGetPerformanceCounter = {implGetPerformanceCounter, "getPerformanceCounter"};

static Status implGetPerformanceFrequency(i16 argc, Value *argv, Value *out) {
  *out = valNumber(SDL_GetPerformanceFrequency());
  return STATUS_OK;
}

static CFunction funcGetPerformanceFrequency = {implGetPerformanceFrequency, "getPerformanceFrequency"};

static Status implGetMouseState(i16 argc, Value *argv, Value *out) {
  ObjPoint *pos = argc > 0 && !isNil(argv[0]) ? asPoint(argv[0]) : NULL;
  if (pos) {
    *out = valNumber(SDL_GetMouseState(&pos->handle->x, &pos->handle->y));
  } else {
    *out = valNumber(SDL_GetMouseState(NULL, NULL));
  }
  return STATUS_OK;
}

static CFunction funcGetMouseState = {implGetMouseState, "getMouseState", 0, 1};

/*
 * ███    ███ ███████ ████████ ██   ██  ██████  ██████  ███████
 * ████  ████ ██         ██    ██   ██ ██    ██ ██   ██ ██
 * ██ ████ ██ █████      ██    ███████ ██    ██ ██   ██ ███████
 * ██  ██  ██ ██         ██    ██   ██ ██    ██ ██   ██      ██
 * ██      ██ ███████    ██    ██   ██  ██████  ██████  ███████
 */

static Status implPointStaticCall(i16 argc, Value *argv, Value *out) {
  ObjPoint *point = newPoint(NULL, NULL);
  point->handle->x = argc > 0 && !isNil(argv[0]) ? asInt(argv[0]) : 0;
  point->handle->y = argc > 1 && !isNil(argv[1]) ? asInt(argv[1]) : 0;
  *out = valPoint(point);
  return STATUS_OK;
}

static CFunction funcPointStaticCall = {implPointStaticCall, "__call__", 0, 2};

static Status implPointGetattr(i16 argc, Value *argv, Value *out) {
  ObjPoint *point = asPoint(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.xString) {
    *out = valNumber(point->handle->x);
  } else if (name == vm.yString) {
    *out = valNumber(point->handle->y);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcPointGetattr = {implPointGetattr, "__getattr__", 1};

static Status implPointSetattr(i16 argc, Value *argv, Value *out) {
  ObjPoint *point = asPoint(argv[-1]);
  String *name = asString(argv[0]);
  Value value = argv[1];
  if (name == vm.xString) {
    point->handle->x = asInt(value);
  } else if (name == vm.yString) {
    point->handle->y = asInt(value);
  } else {
    runtimeError("Field '%s' not found on %s (setattr)", name->chars, getKindName(argv[-1]));
  }
  return STATUS_OK;
}

static CFunction funcPointSetattr = {implPointSetattr, "__setattr__", 2};

static Status implPointSet(i16 argc, Value *argv, Value *out) {
  ObjPoint *point = asPoint(argv[-1]);
  point->handle->x = asInt(argv[0]);
  point->handle->y = asInt(argv[1]);
  return STATUS_OK;
}

static CFunction funcPointSet = {implPointSet, "set", 2};

static Status implFPointStaticCall(i16 argc, Value *argv, Value *out) {
  ObjFPoint *fpoint = newFPoint(NULL, NULL);
  fpoint->handle->x = argc > 0 && !isNil(argv[0]) ? asNumber(argv[0]) : 0;
  fpoint->handle->y = argc > 1 && !isNil(argv[1]) ? asNumber(argv[1]) : 0;
  *out = valFpoint(fpoint);
  return STATUS_OK;
}

static CFunction funcFPointStaticCall = {implFPointStaticCall, "__call__", 0, 2};

static Status implFPointGetattr(i16 argc, Value *argv, Value *out) {
  ObjFPoint *fpoint = asFPoint(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.xString) {
    *out = valNumber(fpoint->handle->x);
  } else if (name == vm.yString) {
    *out = valNumber(fpoint->handle->y);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcFPointGetattr = {implFPointGetattr, "__getattr__", 1};

static Status implFPointSetattr(i16 argc, Value *argv, Value *out) {
  ObjFPoint *fpoint = asFPoint(argv[-1]);
  String *name = asString(argv[0]);
  Value value = argv[1];
  if (name == vm.xString) {
    fpoint->handle->x = asNumber(value);
  } else if (name == vm.yString) {
    fpoint->handle->y = asNumber(value);
  } else {
    runtimeError("Field '%s' not found on %s (setattr)", name->chars, getKindName(argv[-1]));
  }
  return STATUS_OK;
}

static CFunction funcFPointSetattr = {implFPointSetattr, "__setattr__", 2};

static Status implFPointSet(i16 argc, Value *argv, Value *out) {
  ObjFPoint *point = asFPoint(argv[-1]);
  point->handle->x = asNumber(argv[0]);
  point->handle->y = asNumber(argv[1]);
  return STATUS_OK;
}

static CFunction funcFPointSet = {implFPointSet, "set", 2};

static Status implRectStaticCall(i16 argc, Value *argv, Value *out) {
  ObjRect *rect = newRect();
  rect->handle.x = argc > 0 && !isNil(argv[0]) ? asInt(argv[0]) : 0;
  rect->handle.y = argc > 1 && !isNil(argv[1]) ? asInt(argv[1]) : 0;
  rect->handle.w = argc > 2 && !isNil(argv[2]) ? asInt(argv[2]) : 0;
  rect->handle.h = argc > 3 && !isNil(argv[3]) ? asInt(argv[3]) : 0;
  *out = valRect(rect);
  return STATUS_OK;
}

static CFunction funcRectStaticCall = {implRectStaticCall, "__call__", 0, 4};

static Status implRectGetattr(i16 argc, Value *argv, Value *out) {
  ObjRect *rect = asRect(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.xString) {
    *out = valNumber(rect->handle.x);
  } else if (name == vm.yString) {
    *out = valNumber(rect->handle.y);
  } else if (name == vm.wString) {
    *out = valNumber(rect->handle.w);
  } else if (name == vm.hString) {
    *out = valNumber(rect->handle.h);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcRectGetattr = {implRectGetattr, "__getattr__", 1};

static Status implRectSetattr(i16 argc, Value *argv, Value *out) {
  ObjRect *rect = asRect(argv[-1]);
  String *name = asString(argv[0]);
  Value value = argv[1];
  if (name == vm.xString) {
    rect->handle.x = asInt(value);
  } else if (name == vm.yString) {
    rect->handle.y = asInt(value);
  } else if (name == vm.wString) {
    rect->handle.w = asInt(value);
  } else if (name == vm.hString) {
    rect->handle.h = asInt(value);
  } else {
    runtimeError("Field '%s' not found on %s (setattr)", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcRectSetattr = {implRectSetattr, "__setattr__", 2};

static Status implRectSet(i16 argc, Value *argv, Value *out) {
  ObjRect *rect = asRect(argv[-1]);
  if (argc > 0 && !isNil(argv[0])) {
    rect->handle.x = asInt(argv[0]);
  }
  if (argc > 1 && !isNil(argv[1])) {
    rect->handle.y = asInt(argv[1]);
  }
  if (argc > 2 && !isNil(argv[2])) {
    rect->handle.w = asInt(argv[2]);
  }
  if (argc > 3 && !isNil(argv[3])) {
    rect->handle.h = asInt(argv[3]);
  }
  return STATUS_OK;
}

static CFunction funcRectSet = {implRectSet, "set", 0, 4};

static Status implFRectStaticCall(i16 argc, Value *argv, Value *out) {
  ObjFRect *frect = newFRect();
  frect->handle.x = argc > 0 && !isNil(argv[0]) ? asFloat(argv[0]) : 0;
  frect->handle.y = argc > 1 && !isNil(argv[1]) ? asFloat(argv[1]) : 0;
  frect->handle.w = argc > 2 && !isNil(argv[2]) ? asFloat(argv[2]) : 0;
  frect->handle.h = argc > 3 && !isNil(argv[3]) ? asFloat(argv[3]) : 0;
  *out = valFRect(frect);
  return STATUS_OK;
}

static CFunction funcFRectStaticCall = {implFRectStaticCall, "__call__", 0, 4};

static Status implFRectGetattr(i16 argc, Value *argv, Value *out) {
  ObjFRect *frect = asFRect(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.xString) {
    *out = valNumber(frect->handle.x);
  } else if (name == vm.yString) {
    *out = valNumber(frect->handle.y);
  } else if (name == vm.wString) {
    *out = valNumber(frect->handle.w);
  } else if (name == vm.hString) {
    *out = valNumber(frect->handle.h);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcFRectGetattr = {implFRectGetattr, "__getattr__", 1};

static Status implFRectSetattr(i16 argc, Value *argv, Value *out) {
  ObjFRect *frect = asFRect(argv[-1]);
  String *name = asString(argv[0]);
  Value value = argv[1];
  if (name == vm.xString) {
    frect->handle.x = asFloat(value);
  } else if (name == vm.yString) {
    frect->handle.y = asFloat(value);
  } else if (name == vm.wString) {
    frect->handle.w = asFloat(value);
  } else if (name == vm.hString) {
    frect->handle.h = asFloat(value);
  } else {
    runtimeError("Field '%s' not found on %s (setattr)", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcFRectSetattr = {implFRectSetattr, "__setattr__", 2};

static Status implFRectSet(i16 argc, Value *argv, Value *out) {
  ObjFRect *frect = asFRect(argv[-1]);
  if (argc > 0 && !isNil(argv[0])) {
    frect->handle.x = asFloat(argv[0]);
  }
  if (argc > 1 && !isNil(argv[1])) {
    frect->handle.y = asFloat(argv[1]);
  }
  if (argc > 2 && !isNil(argv[2])) {
    frect->handle.w = asFloat(argv[2]);
  }
  if (argc > 3 && !isNil(argv[3])) {
    frect->handle.h = asFloat(argv[3]);
  }
  return STATUS_OK;
}

static CFunction funcFRectSet = {implFRectSet, "set", 0, 4};

static Status implColorStaticCall(i16 argc, Value *argv, Value *out) {
  ObjColor *color = newColor(NULL, NULL);
  color->handle->r = argc > 0 && !isNil(argv[0]) ? asU8(argv[0]) : 0;
  color->handle->g = argc > 1 && !isNil(argv[1]) ? asU8(argv[1]) : 0;
  color->handle->b = argc > 2 && !isNil(argv[2]) ? asU8(argv[2]) : 0;
  color->handle->a = argc > 3 && !isNil(argv[3]) ? asU8(argv[3]) : 255;
  *out = valColor(color);
  return STATUS_OK;
}

static CFunction funcColorStaticCall = {implColorStaticCall, "__call__", 0, 4};

static Status implColorGetattr(i16 argc, Value *argv, Value *out) {
  ObjColor *color = asColor(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.rString) {
    *out = valNumber(color->handle->r);
  } else if (name == vm.gString) {
    *out = valNumber(color->handle->g);
  } else if (name == vm.bString) {
    *out = valNumber(color->handle->b);
  } else if (name == vm.aString) {
    *out = valNumber(color->handle->a);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcColorGetattr = {implColorGetattr, "__getattr__", 1};

static Status implColorSetattr(i16 argc, Value *argv, Value *out) {
  ObjColor *color = asColor(argv[-1]);
  String *name = asString(argv[0]);
  Value value = argv[1];
  if (name == vm.rString) {
    color->handle->r = asU8(value);
  } else if (name == vm.gString) {
    color->handle->g = asU8(value);
  } else if (name == vm.bString) {
    color->handle->b = asU8(value);
  } else if (name == vm.aString) {
    color->handle->a = asU8(value);
  } else {
    runtimeError("Field '%s' not found on %s (setattr)", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcColorSetattr = {implColorSetattr, "__setattr__", 2};

static Status implColorSet(i16 argc, Value *argv, Value *out) {
  ObjColor *color = asColor(argv[-1]);
  if (argc > 0 && !isNil(argv[0])) {
    color->handle->r = asU8(argv[0]);
  }
  if (argc > 1 && !isNil(argv[1])) {
    color->handle->g = asU8(argv[1]);
  }
  if (argc > 2 && !isNil(argv[2])) {
    color->handle->b = asU8(argv[2]);
  }
  if (argc > 3 && !isNil(argv[3])) {
    color->handle->a = asU8(argv[3]);
  }
  return STATUS_OK;
}

static CFunction funcColorSet = {implColorSet, "set", 0, 4};

static Status implVertexStaticCall(i16 argc, Value *argv, Value *out) {
  ObjVertex *vertex = newVertex(NULL, NULL);
  *out = valVertex(vertex);
  return STATUS_OK;
}

static CFunction funcVertexStaticCall = {implVertexStaticCall, "__call__"};

static Status implVertexGetattr(i16 argc, Value *argv, Value *out) {
  ObjVertex *vertex = asVertex(argv[-1]);
  String *name = asString(argv[0]);
  if (name == positionString) {
    *out = valFpoint(vertex->position);
  } else if (name == colorString) {
    *out = valColor(vertex->color);
  } else if (name == texCoordString) {
    *out = valFpoint(vertex->texCoord);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcVertexGetattr = {implVertexGetattr, "__getattr__", 1};

static Status implVertexSetattr(i16 argc, Value *argv, Value *out) {
  ObjVertex *vertex = asVertex(argv[-1]);
  String *name = asString(argv[0]);
  Value value = argv[1];
  if (name == positionString) {
    *vertex->position->handle = *asFPoint(value)->handle;
  } else if (name == colorString) {
    *vertex->color->handle = *asColor(value)->handle;
  } else if (name == texCoordString) {
    *vertex->texCoord->handle = *asFPoint(value)->handle;
  } else {
    runtimeError("Field '%s' not found on %s (setattr)", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcVertexSetattr = {implVertexSetattr, "__setattr__", 2};

static Status implEventStaticCall(i16 argc, Value *argv, Value *out) {
  ObjEvent *event = NEW_NATIVE(ObjEvent, &descriptorEvent);
  *out = valEvent(event);
  return STATUS_OK;
}

static CFunction funcEventStaticCall = {implEventStaticCall, "__call__"};

static Status implEventGetattr(i16 argc, Value *argv, Value *out) {
  ObjEvent *event = asEvent(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.typeString) {
    *out = valNumber(event->handle.type);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcEventGetattr = {implEventGetattr, "__getattr__", 1};

static Status implWindowStaticCall(i16 argc, Value *argv, Value *out) {
  ObjWindow *window;
  String *title = asString(argv[0]);
  int width = asInt(argv[1]);
  int height = asInt(argv[2]);
  if (!newWindow(title->chars, width, height, &window)) {
    return STATUS_ERROR;
  }
  *out = valWindow(window);
  return STATUS_OK;
}

static CFunction funcWindowStaticCall = {implWindowStaticCall, "__call__", 3};

static Status implWindowSetDrawColor(i16 argc, Value *argv, Value *out) {
  ObjWindow *window = asWindow(argv[-1]);
  u8 r = asU8(argv[0]);
  u8 g = asU8(argv[1]);
  u8 b = asU8(argv[2]);
  u8 a = argc > 3 && !isNil(argv[3]) ? asU8(argv[3]) : 255;
  if (SDL_SetRenderDrawColor(window->renderer, r, g, b, a) != 0) {
    return sdlError("SDL_SetRenderDrawColor");
  }
  return STATUS_OK;
}

static CFunction funcWindowSetDrawColor = {implWindowSetDrawColor, "setDrawColor", 3, 4};

static Status implWindowClear(i16 argc, Value *argv, Value *out) {
  ObjWindow *window = asWindow(argv[-1]);
  if (SDL_RenderClear(window->renderer) != 0) {
    return sdlError("SDL_RenderClear");
  }
  return STATUS_OK;
}

static CFunction funcWindowClear = {implWindowClear, "clear"};

static Status implWindowFillRect(i16 argc, Value *argv, Value *out) {
  ObjWindow *window = asWindow(argv[-1]);
  ObjRect *rect = asRect(argv[0]);
  if (SDL_RenderFillRect(window->renderer, &rect->handle) != 0) {
    return sdlError("SDL_RenderFillRect");
  }
  return STATUS_OK;
}

static CFunction funcWindowFillRect = {implWindowFillRect, "fillRect", 1};

static Status implWindowPresent(i16 argc, Value *argv, Value *out) {
  ObjWindow *window = asWindow(argv[-1]);
  SDL_RenderPresent(window->renderer);
  return STATUS_OK;
}

static CFunction funcWindowPresent = {implWindowPresent, "present"};

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
      &funcPollEvent,
      &funcDelay,
      &funcGetPerformanceCounter,
      &funcGetPerformanceFrequency,
      &funcGetMouseState,
      NULL,
  };
  CFunction *pointStaticMethods[] = {
      &funcPointStaticCall,
      NULL,
  };
  CFunction *pointMethods[] = {
      &funcPointGetattr,
      &funcPointSetattr,
      &funcPointSet,
      NULL,
  };
  CFunction *fpointStaticMethods[] = {
      &funcFPointStaticCall,
      NULL,
  };
  CFunction *fpointMethods[] = {
      &funcFPointGetattr,
      &funcFPointSetattr,
      &funcFPointSet,
      NULL,
  };
  CFunction *rectStaticMethods[] = {
      &funcRectStaticCall,
      NULL,
  };
  CFunction *rectMethods[] = {
      &funcRectGetattr,
      &funcRectSetattr,
      &funcRectSet,
      NULL,
  };
  CFunction *frectStaticMethods[] = {
      &funcFRectStaticCall,
      NULL,
  };
  CFunction *frectMethods[] = {
      &funcFRectGetattr,
      &funcFRectSetattr,
      &funcFRectSet,
      NULL,
  };
  CFunction *colorStaticMethods[] = {
      &funcColorStaticCall,
      NULL,
  };
  CFunction *colorMethods[] = {
      &funcColorGetattr,
      &funcColorSetattr,
      &funcColorSet,
      NULL,
  };
  CFunction *vertexStaticMethods[] = {
      &funcVertexStaticCall,
      NULL,
  };
  CFunction *vertexMethods[] = {
      &funcVertexGetattr,
      &funcVertexSetattr,
      NULL,
  };
  CFunction *eventStaticMethods[] = {
      &funcEventStaticCall,
      NULL,
  };
  CFunction *eventMethods[] = {
      &funcEventGetattr,
      NULL,
  };
  CFunction *windowStaticMethods[] = {
      &funcWindowStaticCall,
      NULL,
  };
  CFunction *windowMethods[] = {
      &funcWindowSetDrawColor,
      &funcWindowClear,
      &funcWindowFillRect,
      &funcWindowPresent,
      NULL,
  };
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    return sdlError("SDL_Init");
  }
  moduleRetain(module, valString(positionString = internCString("position")));
  moduleRetain(module, valString(colorString = internCString("color")));
  moduleRetain(module, valString(texCoordString = internCString("texCoord")));
  moduleAddFunctions(module, functions);
  newNativeClass(module, &descriptorPoint, pointMethods, pointStaticMethods);
  newNativeClass(module, &descriptorFPoint, fpointMethods, fpointStaticMethods);
  newNativeClass(module, &descriptorRect, rectMethods, rectStaticMethods);
  newNativeClass(module, &descriptorFRect, frectMethods, frectStaticMethods);
  newNativeClass(module, &descriptorColor, colorMethods, colorStaticMethods);
  newNativeClass(module, &descriptorVertex, vertexMethods, vertexStaticMethods);
  newNativeClass(module, &descriptorEvent, eventMethods, eventStaticMethods);
  newNativeClass(module, &descriptorWindow, windowMethods, windowStaticMethods);
  mapSetN(&module->fields, "QUIT", valNumber(SDL_QUIT));
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

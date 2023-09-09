#include "mtots_m_sdl.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_ENABLE_SDL
#include <SDL2/SDL.h>

#define IS_EVENT(value) (getNativeObjectDescriptor(value) == &descriptorEvent)
#define IS_WINDOW(value) (getNativeObjectDescriptor(value) == &descriptorWindow)

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

static NativeObjectDescriptor descriptorEvent = {
    nopBlacken,
    nopFree,
    sizeof(ObjEvent),
    "Event",
};

static ObjEvent *asEvent(Value value) {
  if (!IS_EVENT(value)) {
    panic("Expected Event but got %s", getKindName(value));
  }
  return (ObjEvent *)AS_OBJ_UNSAFE(value);
}

static void freeWindow(ObjNative *n) {
  ObjWindow *window = (ObjWindow *)n;
  if (window->handle) {
    SDL_DestroyWindow(window->handle);
  }
}

static NativeObjectDescriptor descriptorWindow = {
    nopBlacken,
    freeWindow,
    sizeof(ObjWindow),
    "Window",
};

static Value WINDOW_VAL(ObjWindow *window) {
  return OBJ_VAL_EXPLICIT((Obj *)window);
}

/*
static ObjWindow *asWindow(Value value) {
  if (!IS_WINDOW(value)) {
    panic("Expected Window but got %s", getKindName(value));
  }
  return (ObjWindow *)AS_OBJ_UNSAFE(value);
}
*/

static Status newWindow(const char *title, int width, int height, ObjWindow **out) {
  ObjWindow *window = NEW_NATIVE(ObjWindow, &descriptorWindow);
  if (SDL_CreateWindowAndRenderer(width, height, 0, &window->handle, &window->renderer) != 0) {
    return sdlError("SDL_CreateWindowAndRenderer");
  }
  *out = window;
  return STATUS_OK;
}

static Status implPollEvent(i16 argc, Value *argv, Value *out) {
  ObjEvent *event = asEvent(argv[0]);
  *out = BOOL_VAL(SDL_PollEvent(&event->handle));
  return STATUS_OK;
}

static CFunction funcPollEvent = {implPollEvent, "pollEvent", 1};

static Status implEventStaticCall(i16 argc, Value *argv, Value *out) {
  ObjEvent *event = NEW_NATIVE(ObjEvent, &descriptorEvent);
  *out = OBJ_VAL_EXPLICIT((Obj *)event);
  return STATUS_OK;
}

static CFunction funcEventStaticCall = {implEventStaticCall, "__call__"};

static Status implEventGetattr(i16 argc, Value *argv, Value *out) {
  ObjEvent *event = asEvent(argv[-1]);
  String *name = asString(argv[0]);
  if (name == vm.typeString) {
    *out = NUMBER_VAL(event->handle.type);
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
  *out = WINDOW_VAL(window);
  return STATUS_OK;
}

static CFunction funcWindowStaticCall = {implWindowStaticCall, "__call__", 3};

static Status impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = asModule(args[0]);
  CFunction *functions[] = {
      &funcPollEvent,
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
      NULL,
  };
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    return sdlError("SDL_Init");
  }
  moduleAddFunctions(module, functions);
  newNativeClass(module, &descriptorEvent, eventMethods, eventStaticMethods);
  newNativeClass(module, &descriptorWindow, windowMethods, windowStaticMethods);
  mapSetN(&module->fields, "QUIT", NUMBER_VAL(SDL_QUIT));
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

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
  static Status impl##name##Getattr(i16 argc, Value *argv, Value *out);           \
  static Status impl##name##Setattr(i16 argc, Value *argv, Value *out);           \
  static CFunction func##name##Getattr = {                                        \
      impl##name##Getattr,                                                        \
      "__getattr__",                                                              \
      1,                                                                          \
  };                                                                              \
  static CFunction func##name##Setattr = {                                        \
      impl##name##Setattr,                                                        \
      "__setattr__",                                                              \
      2,                                                                          \
  };                                                                              \
  static CFunction *name##Methods[] = {                                           \
      &func##name##Getattr,                                                       \
      &func##name##Setattr,                                                       \
      NULL,                                                                       \
  };                                                                              \
  static Status impl##name##StaticCall(i16 argc, Value *argv, Value *out) {       \
    *out = val##name(alloc##name());                                              \
    return STATUS_OK;                                                             \
  }                                                                               \
  static CFunction func##name##StaticCall = {impl##name##StaticCall, "__call__"}; \
  static CFunction *name##StaticMethods[] = {                                     \
      &func##name##StaticCall,                                                    \
      NULL,                                                                       \
  };

/** Required functions:
 *   - impl##name##Getattr
 *   - impl##name##Setattr
 */
#define WRAP_SDL_REF_TYPE(name, freeFunc)         \
  static void free##name(ObjNative *n);           \
  WRAP_SDL_COMMON(name, SDL_##name *, free##name) \
  static void free##name(ObjNative *n) {          \
    Obj##name *p = (Obj##name *)n;                \
    if (p->handle) {                              \
      freeFunc(p->handle);                        \
    }                                             \
  }

/** Required functions:
 *   - impl##name##Getattr
 *   - impl##name##Setattr
 */
#define WRAP_SDL_POD_TYPE(name) \
  WRAP_SDL_COMMON(name, SDL_##name, nopFree)

#define DEFINE_NOP_GETATTR(name)                                         \
  static Status impl##name##Getattr(i16 argc, Value *argv, Value *out) { \
    as##name(argv[-1]); /* suppress unused warning */                    \
    fieldNotFoundError(argv[-1], asString(argv[0])->chars);              \
    return STATUS_ERROR;                                                 \
  }

#define DEFINE_NOP_SETATTR(name)                                         \
  static Status impl##name##Setattr(i16 argc, Value *argv, Value *out) { \
    as##name(argv[-1]); /* suppress unused warning */                    \
    fieldNotFoundError(argv[-1], asString(argv[0])->chars);              \
    return STATUS_ERROR;                                                 \
  }

#define PROLOGUE_GETATTR(className)                                           \
  static Status impl##className##Getattr(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                          \
    String *name = asString(argv[0]);
#define EPILOGUE_GETATTR(className)          \
  fieldNotFoundError(argv[-1], name->chars); \
  return STATUS_ERROR;                       \
  }

#define ENTRY_GETATTR_NUMBER(className, attrName) \
  if (name == vm.attrName##String) {              \
    *out = valNumber(owner->handle.attrName);     \
    return STATUS_OK;                             \
  }

#define ENTRY_GETATTR_REF_NUMBER(className, attrName) \
  if (name == vm.attrName##String) {                  \
    *out = valNumber(owner->handle->attrName);        \
    return STATUS_OK;                                 \
  }

#define PROLOGUE_SETATTR(className)                                           \
  static Status impl##className##Setattr(i16 argc, Value *argv, Value *out) { \
    Obj##className *owner = as##className(argv[-1]);                          \
    String *name = asString(argv[0]);                                         \
    Value value = argv[1];
#define EPILOGUE_SETATTR(className)          \
  fieldNotFoundError(argv[-1], name->chars); \
  return STATUS_ERROR;                       \
  }

#define ENTRY_SETATTR_INT(className, attrName) \
  if (name == vm.attrName##String == 0) {      \
    owner->handle.attrName = asInt(value);     \
    return STATUS_OK;                          \
  }

#define ENTRY_SETATTR_FLOAT(className, attrName) \
  if (name == vm.attrName##String == 0) {        \
    owner->handle.attrName = asFloat(value);     \
    return STATUS_OK;                            \
  }

#define DEFINE_GETATTR_REF_N(className, a1) \
  PROLOGUE_GETATTR(className)               \
  ENTRY_GETATTR_REF_NUMBER(className, a1)   \
  EPILOGUE_GETATTR(className)

#define DEFINE_GETATTR_REF_NN(className, a1, a2) \
  PROLOGUE_GETATTR(className)                    \
  ENTRY_GETATTR_REF_NUMBER(className, a1)        \
  ENTRY_GETATTR_REF_NUMBER(className, a2)        \
  EPILOGUE_GETATTR(className)

#define DEFINE_GETATTR_N(className, a1) \
  PROLOGUE_GETATTR(className)           \
  ENTRY_GETATTR_NUMBER(className, a1)   \
  EPILOGUE_GETATTR(className)

#define DEFINE_GETATTR_NN(className, a1, a2) \
  PROLOGUE_GETATTR(className)                \
  ENTRY_GETATTR_NUMBER(className, a1)        \
  ENTRY_GETATTR_NUMBER(className, a2)        \
  EPILOGUE_GETATTR(className)

#define DEFINE_GETATTR_NNN(className, a1, a2, a3) \
  PROLOGUE_GETATTR(className)                     \
  ENTRY_GETATTR_NUMBER(className, a1)             \
  ENTRY_GETATTR_NUMBER(className, a2)             \
  ENTRY_GETATTR_NUMBER(className, a3)             \
  EPILOGUE_GETATTR(className)

#define DEFINE_GETATTR_NNNN(className, a1, a2, a3, a4) \
  PROLOGUE_GETATTR(className)                          \
  ENTRY_GETATTR_NUMBER(className, a1)                  \
  ENTRY_GETATTR_NUMBER(className, a2)                  \
  ENTRY_GETATTR_NUMBER(className, a3)                  \
  ENTRY_GETATTR_NUMBER(className, a4)                  \
  EPILOGUE_GETATTR(className)

#define DEFINE_SETATTR_I(className, a1) \
  PROLOGUE_SETATTR(className)           \
  ENTRY_SETATTR_INT(className, a1)      \
  EPILOGUE_SETATTR(className)

#define DEFINE_SETATTR_II(className, a1, a2) \
  PROLOGUE_SETATTR(className)                \
  ENTRY_SETATTR_INT(className, a1)           \
  ENTRY_SETATTR_INT(className, a2)           \
  EPILOGUE_SETATTR(className)

#define DEFINE_SETATTR_III(className, a1, a2, a3) \
  PROLOGUE_SETATTR(className)                     \
  ENTRY_SETATTR_INT(className, a1)                \
  ENTRY_SETATTR_INT(className, a2)                \
  ENTRY_SETATTR_INT(className, a3)                \
  EPILOGUE_SETATTR(className)

#define DEFINE_SETATTR_IIII(className, a1, a2, a3, a4) \
  PROLOGUE_SETATTR(className)                          \
  ENTRY_SETATTR_INT(className, a1)                     \
  ENTRY_SETATTR_INT(className, a2)                     \
  ENTRY_SETATTR_INT(className, a3)                     \
  ENTRY_SETATTR_INT(className, a4)                     \
  EPILOGUE_SETATTR(className)

#define DEFINE_SETATTR_F(className, a1) \
  PROLOGUE_SETATTR(className)           \
  ENTRY_SETATTR_FLOAT(className, a1)    \
  EPILOGUE_SETATTR(className)

#define DEFINE_SETATTR_FF(className, a1, a2) \
  PROLOGUE_SETATTR(className)                \
  ENTRY_SETATTR_FLOAT(className, a1)         \
  ENTRY_SETATTR_FLOAT(className, a2)         \
  EPILOGUE_SETATTR(className)

#define DEFINE_SETATTR_FFF(className, a1, a2, a3) \
  PROLOGUE_SETATTR(className)                     \
  ENTRY_SETATTR_FLOAT(className, a1)              \
  ENTRY_SETATTR_FLOAT(className, a2)              \
  ENTRY_SETATTR_FLOAT(className, a3)              \
  EPILOGUE_SETATTR(className)

#define DEFINE_SETATTR_FFFF(className, a1, a2, a3, a4) \
  PROLOGUE_SETATTR(className)                          \
  ENTRY_SETATTR_FLOAT(className, a1)                   \
  ENTRY_SETATTR_FLOAT(className, a2)                   \
  ENTRY_SETATTR_FLOAT(className, a3)                   \
  ENTRY_SETATTR_FLOAT(className, a4)                   \
  EPILOGUE_SETATTR(className)

#define ADD_TYPE_TO_MODULE(name) \
  newNativeClass(module, &descriptor##name, name##Methods, name##StaticMethods)

WRAP_SDL_POD_TYPE(Point)
DEFINE_GETATTR_NN(Point, x, y)
DEFINE_SETATTR_II(Point, x, y)

WRAP_SDL_POD_TYPE(FPoint)
DEFINE_GETATTR_NN(FPoint, x, y)
DEFINE_SETATTR_FF(FPoint, x, y)

WRAP_SDL_POD_TYPE(Rect)
DEFINE_GETATTR_NNNN(Rect, x, y, w, h)
DEFINE_SETATTR_IIII(Rect, x, y, w, h)

WRAP_SDL_POD_TYPE(FRect)
DEFINE_GETATTR_NNNN(FRect, x, y, w, h)
DEFINE_SETATTR_FFFF(FRect, x, y, w, h)

WRAP_SDL_POD_TYPE(Color)
DEFINE_GETATTR_NNNN(Color, r, g, b, a)
DEFINE_SETATTR_IIII(Color, r, g, b, a)

WRAP_SDL_REF_TYPE(Surface, SDL_FreeSurface)
DEFINE_GETATTR_REF_NN(Surface, w, h)
DEFINE_NOP_SETATTR(Surface)

WRAP_SDL_REF_TYPE(Window, SDL_DestroyWindow)
DEFINE_NOP_GETATTR(Window)
DEFINE_NOP_SETATTR(Window)

#define isPoint(value) (getNativeObjectDescriptor(value) == &descriptorPoint)
#define isFPoint(value) (getNativeObjectDescriptor(value) == &descriptorFPoint)
#define isRect(value) (getNativeObjectDescriptor(value) == &descriptorRect)
#define isFRect(value) (getNativeObjectDescriptor(value) == &descriptorFRect)
#define isColor(value) (getNativeObjectDescriptor(value) == &descriptorColor)
#define isVertex(value) (getNativeObjectDescriptor(value) == &descriptorVertex)
#define isEvent(value) (getNativeObjectDescriptor(value) == &descriptorEvent)
#define isWindow(value) (getNativeObjectDescriptor(value) == &descriptorWindow)

static Status sdlError(const char *functionName) {
  runtimeError("%s: %s", functionName, SDL_GetError());
  return STATUS_ERROR;
}

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
      NULL,
  };
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    return sdlError("SDL_Init");
  }
  /*
  moduleRetain(module, valString(positionString = internCString("position")));
  moduleRetain(module, valString(colorString = internCString("color")));
  moduleRetain(module, valString(texCoordString = internCString("texCoord")));
  */
  moduleAddFunctions(module, functions);
  ADD_TYPE_TO_MODULE(Point);
  ADD_TYPE_TO_MODULE(FPoint);
  ADD_TYPE_TO_MODULE(Rect);
  ADD_TYPE_TO_MODULE(FRect);
  ADD_TYPE_TO_MODULE(Color);
  ADD_TYPE_TO_MODULE(Surface);
  ADD_TYPE_TO_MODULE(Window);
  /*
  newNativeClass(module, &descriptorPoint, pointMethods, pointStaticMethods);
  newNativeClass(module, &descriptorFPoint, fpointMethods, fpointStaticMethods);
  newNativeClass(module, &descriptorRect, rectMethods, rectStaticMethods);
  newNativeClass(module, &descriptorFRect, frectMethods, frectStaticMethods);
  newNativeClass(module, &descriptorColor, colorMethods, colorStaticMethods);
  newNativeClass(module, &descriptorVertex, vertexMethods, vertexStaticMethods);
  newNativeClass(module, &descriptorEvent, eventMethods, eventStaticMethods);
  newNativeClass(module, &descriptorWindow, windowMethods, windowStaticMethods);
  */
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

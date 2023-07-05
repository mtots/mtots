#include "mtots_m_os.h"

#include "mtots_vm.h"

#include <string.h>
#include <stdlib.h>

#if MTOTS_ASSUME_POSIX
#include <unistd.h>
#include <sys/stat.h>
#define MTOTS_USE_DIRENT 1
#define MTOTS_USE_STAT   1
#endif

#if MTOTS_USE_DIRENT
#include <dirent.h>
#endif

static ubool implGetcwd(i16 argCount, Value *args, Value *out) {
#if MTOTS_USE_DIRENT
  char buffer[MAX_PATH_LENGTH];
  String *path;
  if (getcwd(buffer, MAX_PATH_LENGTH) == NULL) {
    runtimeError("getcwd max path exceeded");
    return UFALSE;
  }
  path = internCString(buffer);
  *out = STRING_VAL(path);
  return UTRUE;
#else
  runtimeError("Unsupported platfom (os.getcwd())");
  return UFALSE;
#endif
}

static CFunction funcGetcwd = {
  implGetcwd, "getcwd",
};

static ubool implGetenv(i16 argCount, Value *args, Value *out) {
  String *name = AS_STRING(args[0]);
  const char *value = getenv(name->chars);
  if (value) {
    *out = STRING_VAL(internCString(value));
  }
  return UTRUE;
}

static CFunction funcGetenv = {
  implGetenv, "getenv", 1, 0, argsStrings,
};

static ubool implIsMacOS(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(
#if MTOTS_ASSUME_MACOS
    UTRUE
#else
    UFALSE
#endif
  );
  return UTRUE;
}

static CFunction funcIsMacOS = { implIsMacOS, "isMacOS" };

static ubool impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = AS_MODULE(args[0]);
  CFunction *cfunctions[] = {
    &funcGetcwd,
    &funcGetenv,
    &funcIsMacOS,
  };
  size_t i;

  for (i = 0; i < sizeof(cfunctions)/sizeof(CFunction*); i++) {
    mapSetN(&module->fields, cfunctions[i]->name, CFUNCTION_VAL(cfunctions[i]));
  }

  mapSetN(&module->fields, "name", STRING_VAL(internCString(OS_NAME)));
  mapSetN(&module->fields, "sep", STRING_VAL(internCString(PATH_SEP_STR)));

  return UTRUE;
}

static CFunction func = { impl, "os", 1 };

void addNativeModuleOs() {
  addNativeModule(&func);
}

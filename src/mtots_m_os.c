#include "mtots_m_os.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static Status implGetcwd(i16 argCount, Value *args, Value *out) {
#if MTOTS_IS_POSIX
  char buffer[MAX_PATH_LENGTH];
  String *path;
  if (getcwd(buffer, MAX_PATH_LENGTH) == NULL) {
    runtimeError("getcwd max path exceeded");
    return STATUS_ERROR;
  }
  path = internCString(buffer);
  *out = STRING_VAL(path);
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.getcwd())");
  return STATUS_ERROR;
#endif
}

static CFunction funcGetcwd = {implGetcwd, "getcwd"};

static Status implGetenv(i16 argCount, Value *args, Value *out) {
  String *name = asString(args[0]);
  const char *value = getenv(name->chars);
  if (value) {
    *out = STRING_VAL(internCString(value));
  }
  return STATUS_OK;
}

static CFunction funcGetenv = {implGetenv, "getenv", 1, 0};

static Status implIsPosix(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_POSIX);
  return STATUS_OK;
}

static CFunction funcIsPosix = {implIsPosix, "isPosix"};

static Status implIsMacOS(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_MACOS);
  return STATUS_OK;
}

static CFunction funcIsMacOS = {implIsMacOS, "isMacOS"};

static Status implIsWindows(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_WINDOWS);
  return STATUS_OK;
}

static CFunction funcIsWindows = {implIsWindows, "isWindows"};

static Status implIsLinux(i16 argCount, Value *args, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_LINUX);
  return STATUS_OK;
}

static CFunction funcIsLinux = {implIsLinux, "isLinux"};

static Status impl(i16 argCount, Value *args, Value *out) {
  ObjModule *module = asModule(args[0]);
  CFunction *functions[] = {
      &funcGetcwd,
      &funcGetenv,
      &funcIsPosix,
      &funcIsMacOS,
      &funcIsWindows,
      &funcIsLinux,
      NULL,
  };

  moduleAddFunctions(module, functions);

  mapSetN(&module->fields, "name", STRING_VAL(internCString(OS_NAME)));
  mapSetN(&module->fields, "sep", STRING_VAL(internCString(PATH_SEP_STR)));

  return STATUS_OK;
}

static CFunction func = {impl, "os", 1};

void addNativeModuleOs(void) {
  addNativeModule(&func);
}

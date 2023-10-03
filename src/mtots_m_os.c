#include "mtots_m_os.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static Status implGetenv(i16 argc, Value *argv, Value *out) {
  String *name = asString(argv[0]);
  const char *value = getenv(name->chars);
  if (value) {
    *out = STRING_VAL(internCString(value));
  }
  return STATUS_OK;
}

static CFunction funcGetenv = {implGetenv, "getenv", 1};

static Status implGetcwd(i16 argc, Value *argv, Value *out) {
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

static Status implChdir(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  String *path = asString(argv[0]);
  if (chdir(path->chars) != 0) {
    runtimeError("chdir: %s", strerror(errno));
    return STATUS_ERROR;
  }
  return STATUS_OK;
#else
  runtimeError("Unsupported platfrom (os.chdir())");
  return STATUS_ERROR;
#endif
}

static CFunction funcChdir = {implChdir, "chdir", 1};

static Status listDirCallback(void *userData, const char *fileName) {
  ObjList *names = (ObjList *)userData;
  if (strcmp(fileName, ".") != 0 && strcmp(fileName, "..") != 0) {
    Value item = STRING_VAL(internCString(fileName));
    push(item);
    listAppend(names, item);
    pop(); /* item */
  }
  return STATUS_OK;
}

static Status implListdir(i16 argc, Value *argv, Value *out) {
  const char *path = argc > 0 && !isNil(argv[0]) ? asString(argv[0])->chars : ".";
  ObjList *names = newList(0);
  push(LIST_VAL(names));
  if (!listDirectory(path, names, listDirCallback)) {
    pop(); /* names */
    return STATUS_ERROR;
  }
  pop(); /* names */
  *out = LIST_VAL(names);
  return STATUS_OK;
}

static CFunction funcListdir = {implListdir, "listdir", 0, 1};

static Status implIsPosix(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_POSIX);
  return STATUS_OK;
}

static CFunction funcIsPosix = {implIsPosix, "isPosix"};

static Status implIsDarwin(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_DARWIN);
  return STATUS_OK;
}

static CFunction funcIsDarwin = {implIsDarwin, "isDarwin"};

static Status implIsMacOS(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_MACOS);
  return STATUS_OK;
}

static CFunction funcIsMacOS = {implIsMacOS, "isMacOS"};

static Status implIsWindows(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_WINDOWS);
  return STATUS_OK;
}

static CFunction funcIsWindows = {implIsWindows, "isWindows"};

static Status implIsLinux(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_LINUX);
  return STATUS_OK;
}

static CFunction funcIsLinux = {implIsLinux, "isLinux"};

static Status implIsIPhone(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_IPHONE);
  return STATUS_OK;
}

static CFunction funcIsIPhone = {implIsIPhone, "isIPhone"};

static Status implIsAndroid(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_ANDROID);
  return STATUS_OK;
}

static CFunction funcIsAndroid = {implIsAndroid, "isAndroid"};

static Status implIsEmscripten(i16 argc, Value *argv, Value *out) {
  *out = BOOL_VAL(MTOTS_IS_EMSCRIPTEN);
  return STATUS_OK;
}

static CFunction funcIsEmscripten = {implIsEmscripten, "isEmscripten"};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcGetenv,
      &funcGetcwd,
      &funcChdir,
      &funcListdir,
      &funcIsPosix,
      &funcIsDarwin,
      &funcIsMacOS,
      &funcIsWindows,
      &funcIsLinux,
      &funcIsIPhone,
      &funcIsAndroid,
      &funcIsEmscripten,
      NULL,
  };

  moduleAddFunctions(module, functions);

  mapSetN(&module->fields, "name", STRING_VAL(internCString(MTOTS_OS_NAME)));
  mapSetN(&module->fields, "sep", STRING_VAL(internCString(PATH_SEP_STR)));

  {
    ObjModule *osPathModule;
    String *moduleNameString = internCString("os.path");
    push(STRING_VAL(moduleNameString));
    if (!importModule(moduleNameString)) {
      return STATUS_ERROR;
    }
    osPathModule = asModule(pop()); /* module (from importModule) */
    pop();                          /* moduleName */

    mapSetN(&module->fields, "path", MODULE_VAL(osPathModule));
  }

  return STATUS_OK;
}

static CFunction func = {impl, "os", 1};

void addNativeModuleOs(void) {
  addNativeModule(&func);
}

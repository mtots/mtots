#include "mtots_m_os_path.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

static Status implIsfile(i16 argc, Value *argv, Value *out) {
  String *path = asString(argv[0]);
  *out = valBool(isFile(path->chars));
  return STATUS_OK;
}

static CFunction funcIsfile = {implIsfile, "isfile", 1};

static Status implIsdir(i16 argc, Value *argv, Value *out) {
  String *path = asString(argv[0]);
  *out = valBool(isDirectory(path->chars));
  return STATUS_OK;
}

static CFunction funcIsdir = {implIsdir, "isdir", 1};

static Status implGetsize(i16 argc, Value *argv, Value *out) {
  String *path = asString(argv[0]);
  size_t fileSize;
  if (!getFileSize(path->chars, &fileSize)) {
    return STATUS_ERROR;
  }
  *out = valNumber(fileSize);
  return STATUS_OK;
}

static CFunction funcGetsize = {implGetsize, "getsize", 1};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcIsfile,
      &funcIsdir,
      &funcGetsize,
      NULL,
  };

  moduleAddFunctions(module, functions);

  mapSetN(&module->fields, "sep", valString(internCString(PATH_SEP_STR)));

  return STATUS_OK;
}

static CFunction func = {impl, "os.path", 1};

void addNativeModuleOsPath(void) {
  addNativeModule(&func);
}

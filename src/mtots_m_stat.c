#include "mtots_m_stat.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if MTOTS_IS_POSIX
#define MAKE_FUNC(fn)                                         \
  static Status impl##fn(i16 argc, Value *argv, Value *out) { \
    mode_t mode = (mode_t)asInt(argv[0]);                     \
    *out = valBool(!!fn(mode));                              \
    return STATUS_OK;                                         \
  }                                                           \
  static CFunction func##fn = {impl##fn, #fn, 1}
#else
#define MAKE_FUNC(fn)                                         \
  static Status impl##fn(i16 argc, Value *argv, Value *out) { \
    runtimeError("Unsupported platfom (stat." #fn "())");     \
    return STATUS_ERROR;                                      \
  }                                                           \
  static CFunction func##fn = {impl##fn, #fn, 1}
#endif

MAKE_FUNC(S_ISDIR);
MAKE_FUNC(S_ISCHR);
MAKE_FUNC(S_ISBLK);
MAKE_FUNC(S_ISREG);
MAKE_FUNC(S_ISFIFO);
MAKE_FUNC(S_ISLNK);
MAKE_FUNC(S_ISSOCK);

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcS_ISDIR,
      &funcS_ISCHR,
      &funcS_ISBLK,
      &funcS_ISREG,
      &funcS_ISFIFO,
      &funcS_ISLNK,
      &funcS_ISSOCK,
      NULL,
  };

  moduleAddFunctions(module, functions);

#if MTOTS_IS_POSIX
  mapSetN(&module->fields, "S_IRWXU", valNumber(S_IRWXU));
  mapSetN(&module->fields, "S_IRUSR", valNumber(S_IRUSR));
  mapSetN(&module->fields, "S_IWUSR", valNumber(S_IWUSR));
  mapSetN(&module->fields, "S_IXUSR", valNumber(S_IXUSR));
  mapSetN(&module->fields, "S_IRWXG", valNumber(S_IRWXG));
  mapSetN(&module->fields, "S_IRGRP", valNumber(S_IRGRP));
  mapSetN(&module->fields, "S_IWGRP", valNumber(S_IWGRP));
  mapSetN(&module->fields, "S_IXGRP", valNumber(S_IXGRP));
  mapSetN(&module->fields, "S_IRWXO", valNumber(S_IRWXO));
  mapSetN(&module->fields, "S_IROTH", valNumber(S_IROTH));
  mapSetN(&module->fields, "S_IWOTH", valNumber(S_IWOTH));
  mapSetN(&module->fields, "S_IXOTH", valNumber(S_IXOTH));
#endif

  return STATUS_OK;
}

static CFunction func = {impl, "stat", 1};

void addNativeModuleStat(void) {
  addNativeModule(&func);
}

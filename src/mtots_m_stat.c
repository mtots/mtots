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
    *out = BOOL_VAL(!!fn(mode));                              \
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
  mapSetN(&module->fields, "S_IRWXU", NUMBER_VAL(S_IRWXU));
  mapSetN(&module->fields, "S_IRUSR", NUMBER_VAL(S_IRUSR));
  mapSetN(&module->fields, "S_IWUSR", NUMBER_VAL(S_IWUSR));
  mapSetN(&module->fields, "S_IXUSR", NUMBER_VAL(S_IXUSR));
  mapSetN(&module->fields, "S_IRWXG", NUMBER_VAL(S_IRWXG));
  mapSetN(&module->fields, "S_IRGRP", NUMBER_VAL(S_IRGRP));
  mapSetN(&module->fields, "S_IWGRP", NUMBER_VAL(S_IWGRP));
  mapSetN(&module->fields, "S_IXGRP", NUMBER_VAL(S_IXGRP));
  mapSetN(&module->fields, "S_IRWXO", NUMBER_VAL(S_IRWXO));
  mapSetN(&module->fields, "S_IROTH", NUMBER_VAL(S_IROTH));
  mapSetN(&module->fields, "S_IWOTH", NUMBER_VAL(S_IWOTH));
  mapSetN(&module->fields, "S_IXOTH", NUMBER_VAL(S_IXOTH));
#endif

  return STATUS_OK;
}

static CFunction func = {impl, "stat", 1};

void addNativeModuleStat(void) {
  addNativeModule(&func);
}

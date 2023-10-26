#include "mtots_m_os.h"

#include <stdlib.h>
#include <string.h>

#include "mtots.h"

#if MTOTS_IS_POSIX
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define isStatResult(v) (getNativeObjectDescriptor(v) == &descriptorStatResult)

#if MTOTS_IS_POSIX
static String *string_st_mode;
static String *string_st_ino;
static String *string_st_dev;
static String *string_st_nlink;
static String *string_st_uid;
static String *string_st_gid;
static String *string_st_size;
#endif

typedef struct ObjStatResult {
  ObjNative obj;
#if MTOTS_IS_POSIX
  struct stat handle;
#else
  int handle;
#endif
} ObjStatResult;

static NativeObjectDescriptor descriptorStatResult = {
    nopBlacken,
    nopFree,
    sizeof(ObjStatResult),
    "StatResult",
};

static Value valStatResult(ObjStatResult *sr) {
  return valObjExplicit((Obj *)sr);
}

static ObjStatResult *asStatResult(Value value) {
  if (!isStatResult(value)) {
    panic("Expected StatResult but got %s", getKindName(value));
  }
  return (ObjStatResult *)value.as.obj;
}

static ObjStatResult *newStatResult() {
  ObjStatResult *sr = NEW_NATIVE(ObjStatResult, &descriptorStatResult);
  memset(&sr->handle, 0, sizeof(sr->handle));
  return sr;
}

static Status implStatResultStaticCall(i16 argc, Value *argv, Value *out) {
  *out = valStatResult(newStatResult());
  return STATUS_OK;
}

static CFunction funcStatResultStaticCall = {implStatResultStaticCall, "__call__"};

static Status implStatResultGetattr(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  ObjStatResult *sr = asStatResult(argv[-1]);
  String *name = asString(argv[0]);
  if (name == string_st_mode) {
    *out = valNumber(sr->handle.st_mode);
  } else if (name == string_st_ino) {
    /* NOTE: st_ino may be a 64-bit integer...
     * TODO: figure out a way not to lose precision by using something
     * other than a double */
    *out = valNumber(sr->handle.st_ino);
  } else if (name == string_st_dev) {
    *out = valNumber(sr->handle.st_dev);
  } else if (name == string_st_nlink) {
    *out = valNumber(sr->handle.st_nlink);
  } else if (name == string_st_uid) {
    *out = valNumber(sr->handle.st_uid);
  } else if (name == string_st_gid) {
    *out = valNumber(sr->handle.st_gid);
  } else if (name == string_st_size) {
    *out = valNumber(sr->handle.st_size);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
#else
  runtimeError("Field '%s' not found on %s",
               asString(argv[0])->chars,
               getKindName(argv[-1]));
  return STATUS_ERROR;
#endif
}

static CFunction funcStatResultGetattr = {implStatResultGetattr, "__getattr__", 1};

static Status implGetlogin(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  *out = valString(internCString(getlogin()));
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.getlogin())");
  return STATUS_ERROR;
#endif
}

static CFunction funcGetlogin = {implGetlogin, "getlogin"};

static Status implGetpid(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  *out = valNumber((double)(pid_t)getpid());
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.getpid())");
  return STATUS_ERROR;
#endif
}

static CFunction funcGetpid = {implGetpid, "getpid"};

static Status implGetppid(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  *out = valNumber((double)(pid_t)getppid());
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.getppid())");
  return STATUS_ERROR;
#endif
}

static CFunction funcGetppid = {implGetppid, "getppid"};

static Status implGetuid(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  *out = valNumber((double)(uid_t)getuid());
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.getuid())");
  return STATUS_ERROR;
#endif
}

static CFunction funcGetuid = {implGetuid, "getuid"};

static Status implGeteuid(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  *out = valNumber((double)(uid_t)geteuid());
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.geteuid())");
  return STATUS_ERROR;
#endif
}

static CFunction funcGeteuid = {implGeteuid, "geteuid"};

static Status implGetenv(i16 argc, Value *argv, Value *out) {
  String *name = asString(argv[0]);
  const char *value = getenv(name->chars);
  if (value) {
    *out = valString(internCString(value));
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
  *out = valString(path);
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
    Value item = valString(internCString(fileName));
    push(item);
    listAppend(names, item);
    pop(); /* item */
  }
  return STATUS_OK;
}

static Status implListdir(i16 argc, Value *argv, Value *out) {
  const char *path = argc > 0 && !isNil(argv[0]) ? asString(argv[0])->chars : ".";
  ObjList *names = newList(0);
  push(valList(names));
  if (!listDirectory(path, names, listDirCallback)) {
    pop(); /* names */
    return STATUS_ERROR;
  }
  pop(); /* names */
  *out = valList(names);
  return STATUS_OK;
}

static CFunction funcListdir = {implListdir, "listdir", 0, 1};

static Status implIsPosix(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_POSIX);
  return STATUS_OK;
}

static CFunction funcIsPosix = {implIsPosix, "isPosix"};

static Status implIsDarwin(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_DARWIN);
  return STATUS_OK;
}

static CFunction funcIsDarwin = {implIsDarwin, "isDarwin"};

static Status implIsMacOS(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_MACOS);
  return STATUS_OK;
}

static CFunction funcIsMacOS = {implIsMacOS, "isMacOS"};

static Status implIsWindows(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_WINDOWS);
  return STATUS_OK;
}

static CFunction funcIsWindows = {implIsWindows, "isWindows"};

static Status implIsLinux(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_LINUX);
  return STATUS_OK;
}

static CFunction funcIsLinux = {implIsLinux, "isLinux"};

static Status implIsIPhone(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_IPHONE);
  return STATUS_OK;
}

static CFunction funcIsIPhone = {implIsIPhone, "isIPhone"};

static Status implIsAndroid(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_ANDROID);
  return STATUS_OK;
}

static CFunction funcIsAndroid = {implIsAndroid, "isAndroid"};

static Status implIsEmscripten(i16 argc, Value *argv, Value *out) {
  *out = valBool(MTOTS_IS_EMSCRIPTEN);
  return STATUS_OK;
}

static CFunction funcIsEmscripten = {implIsEmscripten, "isEmscripten"};

static Status implOpen(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  int fd;
  const char *path = asString(argv[0])->chars;
  int flags = asInt(argv[1]);
  mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO; /* default */
  if (argc > 2 && !isNil(argv[2])) {
    mode = (mode_t)asU32(argv[2]);
  }
  fd = open(path, flags, mode);
  if (fd == -1) {
    runtimeError("open(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valNumber(fd);
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.open())");
  return STATUS_ERROR;
#endif
}

static const char *argsOpen[] = {
    "path",
    "flags",
    "mode",
    NULL,
};

static CFunction funcOpen = {
    implOpen,
    "open",
    2,
    sizeof(argsOpen) / sizeof(argsOpen[0]) - 1,
    argsOpen,
};

static Status implClose(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  int fd = asInt(argv[0]);
  if (close(fd) != 0) {
    runtimeError("close(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.close())");
  return STATUS_ERROR;
#endif
}

static CFunction funcClose = {implClose, "close", 1};

static Status implFstat(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  int fd = asInt(argv[0]);
  ObjStatResult *sr = argc > 1 && !isNil(argv[1]) ? asStatResult(argv[1]) : newStatResult();
  if (fstat(fd, &sr->handle) != 0) {
    runtimeError("fstat(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valStatResult(sr);
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.fstat())");
  return STATUS_ERROR;
#endif
}

static CFunction funcFstat = {implFstat, "fstat", 1, 2};

static Status implStat(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  const char *path = asString(argv[0])->chars;
  ObjStatResult *sr = argc > 1 && !isNil(argv[1]) ? asStatResult(argv[1]) : newStatResult();
  if (stat(path, &sr->handle) != 0) {
    runtimeError("stat(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valStatResult(sr);
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.stat())");
  return STATUS_ERROR;
#endif
}

static CFunction funcStat = {implStat, "stat", 1, 2};

static Status implSysconf(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  int name = asInt(argv[0]);
  long result;
  errno = 0;
  result = sysconf(name);
  if (errno != 0) {
    runtimeError("sysconf(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valNumber(result);
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (os.sysconf())");
  return STATUS_ERROR;
#endif
}

static CFunction funcSysconf = {implSysconf, "sysconf", 1};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *statResultStaticMethods[] = {
      &funcStatResultStaticCall,
      NULL,
  };
  CFunction *statResultMethods[] = {
      &funcStatResultGetattr,
      NULL,
  };
  CFunction *functions[] = {
      &funcGetlogin,
      &funcGetpid,
      &funcGetppid,
      &funcGetuid,
      &funcGeteuid,
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
      &funcOpen,
      &funcClose,
      &funcFstat,
      &funcStat,
      &funcSysconf,
      NULL,
  };

  moduleAddFunctions(module, functions);
  newNativeClass(
      module,
      &descriptorStatResult,
      statResultMethods,
      statResultStaticMethods);

#if MTOTS_IS_POSIX
  string_st_mode = moduleRetainCString(module, "st_mode");
  string_st_ino = moduleRetainCString(module, "st_ino");
  string_st_dev = moduleRetainCString(module, "st_dev");
  string_st_nlink = moduleRetainCString(module, "st_nlink");
  string_st_uid = moduleRetainCString(module, "st_uid");
  string_st_gid = moduleRetainCString(module, "st_gid");
  string_st_size = moduleRetainCString(module, "st_size");
#endif

#if MTOTS_IS_POSIX
  mapSetN(&module->fields, "O_RDONLY", valNumber(O_RDONLY));
  mapSetN(&module->fields, "O_WRONLY", valNumber(O_WRONLY));
  mapSetN(&module->fields, "O_RDWR", valNumber(O_RDWR));
  mapSetN(&module->fields, "O_APPEND", valNumber(O_APPEND));
  mapSetN(&module->fields, "O_CREAT", valNumber(O_CREAT));
  mapSetN(&module->fields, "O_EXCL", valNumber(O_EXCL));
  mapSetN(&module->fields, "O_TRUNC", valNumber(O_TRUNC));
#endif

  mapSetN(&module->fields, "name", valString(internCString(MTOTS_OS_NAME)));
  mapSetN(&module->fields, "sep", valString(internCString(PATH_SEP_STR)));

  {
    ObjModule *osPathModule;
    String *moduleNameString = internCString("os.path");
    push(valString(moduleNameString));
    if (!importModule(moduleNameString)) {
      return STATUS_ERROR;
    }
    osPathModule = asModule(pop()); /* module (from importModule) */
    pop();                          /* moduleName */

    mapSetN(&module->fields, "path", valModule(osPathModule));
  }

#if MTOTS_IS_POSIX
  {
    Map map;
    ubool gcFlag;
    locallyPauseGC(&gcFlag);
    initMap(&map);
    mapSetN(&map, "SC_ARG_MAX", valNumber(_SC_ARG_MAX));
    mapSetN(&map, "SC_CHILD_MAX", valNumber(_SC_CHILD_MAX));
    mapSetN(&map, "SC_CLK_TCK", valNumber(_SC_CLK_TCK));
    mapSetN(&map, "SC_NGROUPS_MAX", valNumber(_SC_NGROUPS_MAX));
    mapSetN(&map, "SC_OPEN_MAX", valNumber(_SC_OPEN_MAX));
    mapSetN(&map, "SC_JOB_CONTROL", valNumber(_SC_JOB_CONTROL));
    mapSetN(&map, "SC_SAVED_IDS", valNumber(_SC_SAVED_IDS));
    mapSetN(&map, "SC_VERSION", valNumber(_SC_VERSION));
    mapSetN(&map, "SC_BC_BASE_MAX", valNumber(_SC_BC_BASE_MAX));
    mapSetN(&map, "SC_BC_DIM_MAX", valNumber(_SC_BC_DIM_MAX));
    mapSetN(&map, "SC_BC_SCALE_MAX", valNumber(_SC_BC_SCALE_MAX));
    mapSetN(&map, "SC_BC_STRING_MAX", valNumber(_SC_BC_STRING_MAX));
    mapSetN(&map, "SC_COLL_WEIGHTS_MAX", valNumber(_SC_COLL_WEIGHTS_MAX));
    mapSetN(&map, "SC_EXPR_NEST_MAX", valNumber(_SC_EXPR_NEST_MAX));
    mapSetN(&map, "SC_LINE_MAX", valNumber(_SC_LINE_MAX));
    mapSetN(&map, "SC_RE_DUP_MAX", valNumber(_SC_RE_DUP_MAX));
    mapSetN(&map, "SC_2_VERSION", valNumber(_SC_2_VERSION));
    mapSetN(&map, "SC_2_C_BIND", valNumber(_SC_2_C_BIND));
    mapSetN(&map, "SC_2_C_DEV", valNumber(_SC_2_C_DEV));
    mapSetN(&map, "SC_2_CHAR_TERM", valNumber(_SC_2_CHAR_TERM));
    mapSetN(&map, "SC_2_FORT_DEV", valNumber(_SC_2_FORT_DEV));
    mapSetN(&map, "SC_2_FORT_RUN", valNumber(_SC_2_FORT_RUN));
    mapSetN(&map, "SC_2_LOCALEDEF", valNumber(_SC_2_LOCALEDEF));
    mapSetN(&map, "SC_2_SW_DEV", valNumber(_SC_2_SW_DEV));
    mapSetN(&map, "SC_2_UPE", valNumber(_SC_2_UPE));
    mapSetN(&map, "SC_STREAM_MAX", valNumber(_SC_STREAM_MAX));
    mapSetN(&map, "SC_TZNAME_MAX", valNumber(_SC_TZNAME_MAX));
    mapSetN(&map, "SC_ASYNCHRONOUS_IO", valNumber(_SC_ASYNCHRONOUS_IO));
    mapSetN(&map, "SC_PAGESIZE", valNumber(_SC_PAGESIZE));
    mapSetN(&map, "SC_MEMLOCK", valNumber(_SC_MEMLOCK));
    mapSetN(&map, "SC_MEMLOCK_RANGE", valNumber(_SC_MEMLOCK_RANGE));
    mapSetN(&map, "SC_MEMORY_PROTECTION", valNumber(_SC_MEMORY_PROTECTION));
    mapSetN(&map, "SC_MESSAGE_PASSING", valNumber(_SC_MESSAGE_PASSING));
    mapSetN(&map, "SC_PRIORITIZED_IO", valNumber(_SC_PRIORITIZED_IO));
    mapSetN(&map, "SC_PRIORITY_SCHEDULING", valNumber(_SC_PRIORITY_SCHEDULING));
    mapSetN(&map, "SC_REALTIME_SIGNALS", valNumber(_SC_REALTIME_SIGNALS));
    mapSetN(&map, "SC_SEMAPHORES", valNumber(_SC_SEMAPHORES));
    mapSetN(&map, "SC_FSYNC", valNumber(_SC_FSYNC));
    mapSetN(&map, "SC_SHARED_MEMORY_OBJECTS", valNumber(_SC_SHARED_MEMORY_OBJECTS));
    mapSetN(&map, "SC_SYNCHRONIZED_IO", valNumber(_SC_SYNCHRONIZED_IO));
    mapSetN(&map, "SC_TIMERS", valNumber(_SC_TIMERS));
    mapSetN(&map, "SC_AIO_LISTIO_MAX", valNumber(_SC_AIO_LISTIO_MAX));
    mapSetN(&map, "SC_AIO_MAX", valNumber(_SC_AIO_MAX));
    mapSetN(&map, "SC_AIO_PRIO_DELTA_MAX", valNumber(_SC_AIO_PRIO_DELTA_MAX));
    mapSetN(&map, "SC_DELAYTIMER_MAX", valNumber(_SC_DELAYTIMER_MAX));
    mapSetN(&map, "SC_MQ_OPEN_MAX", valNumber(_SC_MQ_OPEN_MAX));
    mapSetN(&map, "SC_MAPPED_FILES", valNumber(_SC_MAPPED_FILES));
    mapSetN(&map, "SC_RTSIG_MAX", valNumber(_SC_RTSIG_MAX));
    mapSetN(&map, "SC_SEM_NSEMS_MAX", valNumber(_SC_SEM_NSEMS_MAX));
    mapSetN(&map, "SC_SEM_VALUE_MAX", valNumber(_SC_SEM_VALUE_MAX));
    mapSetN(&map, "SC_SIGQUEUE_MAX", valNumber(_SC_SIGQUEUE_MAX));
    mapSetN(&map, "SC_TIMER_MAX", valNumber(_SC_TIMER_MAX));
    mapSetN(&module->fields, "sysconf_names", valFrozenDict(newFrozenDict(&map)));
    freeMap(&map);
    locallyUnpauseGC(gcFlag);
  }
#endif

  return STATUS_OK;
}

static CFunction func = {impl, "os", 1};

void addNativeModuleOs(void) {
  addNativeModule(&func);
}

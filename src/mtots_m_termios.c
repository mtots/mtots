#include "mtots_m_termios.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

#define isTermios(value) (getNativeObjectDescriptor(value) == &descriptorTermios)

#if MTOTS_IS_POSIX
static String *string_c_iflag;
static String *string_c_oflag;
static String *string_c_cflag;
static String *string_c_lflag;
#endif

typedef struct ObjTermios {
  ObjNative obj;
#if MTOTS_IS_POSIX
  struct termios handle;
#else
  int handle;
#endif
} ObjTermios;

static ObjTermios *defaultTermios;

static NativeObjectDescriptor descriptorTermios = {
    nopBlacken,
    nopFree,
    sizeof(ObjTermios),
    "Termios",
};

static ObjTermios *newTermios() {
  ObjTermios *t = NEW_NATIVE(ObjTermios, &descriptorTermios);
  memset(&t->handle, 0, sizeof(t->handle));
  return t;
}

static ObjTermios *asTermios(Value value) {
  if (!isTermios(value)) {
    panic("Expected Termios but got %s", getKindName(value));
  }
  return (ObjTermios *)AS_OBJ_UNSAFE(value);
}

static Value valTermios(ObjTermios *t) {
  return valObjExplicit((Obj *)t);
}

static Status implTermiosStaticCall(i16 argc, Value *argv, Value *out) {
  *out = valTermios(newTermios());
  return STATUS_OK;
}

static CFunction funcTermiosStaticCall = {implTermiosStaticCall, "__call__"};

static Status implTermiosGetattr(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  ObjTermios *t = asTermios(argv[-1]);
  String *name = asString(argv[0]);
  if (name == string_c_iflag) {
    *out = valNumber(t->handle.c_iflag);
  } else if (name == string_c_oflag) {
    *out = valNumber(t->handle.c_oflag);
  } else if (name == string_c_cflag) {
    *out = valNumber(t->handle.c_cflag);
  } else if (name == string_c_lflag) {
    *out = valNumber(t->handle.c_lflag);
  } else {
    fieldNotFoundError(argv[-1], name->chars);
    return STATUS_ERROR;
  }
  return STATUS_OK;
#else
  fieldNotFoundError(argv[-1], asString(argv[0])->chars);
  return STATUS_ERROR;
#endif
}

static CFunction funcTermiosGetattr = {implTermiosGetattr, "__getattr__", 1};

static Status implTermiosSetattr(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  ObjTermios *t = asTermios(argv[-1]);
  String *name = asString(argv[0]);
  Value value = argv[1];
  if (name == string_c_iflag) {
    t->handle.c_iflag = asU32(value);
  } else if (name == string_c_oflag) {
    t->handle.c_oflag = asU32(value);
  } else if (name == string_c_cflag) {
    t->handle.c_cflag = asU32(value);
  } else if (name == string_c_lflag) {
    t->handle.c_lflag = asU32(value);
  } else {
    fieldNotFoundError(argv[-1], name->chars);
    return STATUS_ERROR;
  }
  return STATUS_OK;
#else
  fieldNotFoundError(argv[-1], asString(argv[0])->chars);
  return STATUS_ERROR;
#endif
}

static CFunction funcTermiosSetattr = {implTermiosSetattr, "__setattr__", 2};

static Status implTcgetattr(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  int fd = asInt(argv[0]);
  ObjTermios *tios = argc > 1 && !isNil(argv[1]) ? asTermios(argv[1]) : defaultTermios;
  if (0 != tcgetattr(fd, &tios->handle)) {
    runtimeError("tcgetattr(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valTermios(tios);
  return STATUS_OK;
#else
  runtimeError("tcgetattr() is not supported on this platform");
  return STATUS_ERROR;
#endif
}

static CFunction funcTcgetattr = {implTcgetattr, "tcgetattr", 1, 2};

static Status implTcsetattr(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  int fd = asInt(argv[0]);
  int when = asInt(argv[1]);
  ObjTermios *tios = asTermios(argv[2]);
  if (0 != tcsetattr(fd, when, &tios->handle)) {
    runtimeError("tcsetattr(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valTermios(tios);
  return STATUS_OK;
#else
  runtimeError("tcsetattr() is not supported on this platform");
  return STATUS_ERROR;
#endif
}

static CFunction funcTcsetattr = {implTcsetattr, "tcsetattr", 3};

#if MTOTS_IS_POSIX
static struct termios originalTermios;
static void restoreOriginalTermios(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
}
static Status implRestoreTermios(i16 argc, Value *argv, Value *out) {
  restoreOriginalTermios();
  return STATUS_OK;
}
static CFunction funcRestoreTermios = {
    implRestoreTermios,
    "restoreTermios",
};
#endif

static Status implRestoreAtExit(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  if (0 != tcgetattr(STDIN_FILENO, &originalTermios)) {
    runtimeError("tcgetattr(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  registerMtotsAtExitCallback(valCFunction(&funcRestoreTermios));
  return STATUS_OK;
#else
  runtimeError("termios.restoreAtExit() is not supported on this platform");
  return STATUS_ERROR;
#endif
}

static CFunction funcRestoreAtExit = {implRestoreAtExit, "restoreAtExit"};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *termiosStaticMethods[] = {
      &funcTermiosStaticCall,
      NULL,
  };
  CFunction *termiosMethods[] = {
      &funcTermiosGetattr,
      &funcTermiosSetattr,
      NULL,
  };
  CFunction *functions[] = {
      &funcTcgetattr,
      &funcTcsetattr,
      &funcRestoreAtExit,
      NULL,
  };

  moduleRetain(module, valTermios(defaultTermios = newTermios()));

  moduleAddFunctions(module, functions);

  newNativeClass(
      module,
      &descriptorTermios,
      termiosMethods,
      termiosStaticMethods);

#if MTOTS_IS_POSIX
  string_c_iflag = moduleRetainCString(module, "c_iflag");
  string_c_oflag = moduleRetainCString(module, "c_oflag");
  string_c_cflag = moduleRetainCString(module, "c_cflag");
  string_c_lflag = moduleRetainCString(module, "c_lflag");

  /* For use with tcsetattr */
  mapSetN(&module->fields, "TCSANOW", valNumber(TCSANOW));
  mapSetN(&module->fields, "TCSADRAIN", valNumber(TCSADRAIN));
  mapSetN(&module->fields, "TCSAFLUSH", valNumber(TCSAFLUSH));

  /* === input modes === */

  /* Signal interrupt on break. */
  mapSetN(&module->fields, "BRKINT", valNumber(BRKINT));

  /* Map CR to NL on input. */
  mapSetN(&module->fields, "ICRNL", valNumber(ICRNL));

  /* Ignore break condition. */
  mapSetN(&module->fields, "IGNBRK", valNumber(IGNBRK));

  /* Ignore CR. */
  mapSetN(&module->fields, "IGNCR", valNumber(IGNCR));

  /* Ignore characters with parity errors. */
  mapSetN(&module->fields, "IGNPAR", valNumber(IGNPAR));

  /* Map NL to CR on input. */
  mapSetN(&module->fields, "INLCR", valNumber(INLCR));

  /* Enable input parity check. */
  mapSetN(&module->fields, "INPCK", valNumber(INPCK));

  /* Strip character. */
  mapSetN(&module->fields, "ISTRIP", valNumber(ISTRIP));

  /* [XSI] [Option Start] Enable any character to restart output. [Option End] */
  mapSetN(&module->fields, "IXANY", valNumber(IXANY));

  /* Enable start/stop input control. */
  mapSetN(&module->fields, "IXOFF", valNumber(IXOFF));

  /* Enable start/stop output control. */
  mapSetN(&module->fields, "IXON", valNumber(IXON));

  /* Mark parity errors. */
  mapSetN(&module->fields, "PARMRK", valNumber(PARMRK));

  /* === local modes === */

  /* Enable echo. */
  mapSetN(&module->fields, "ECHO", valNumber(ECHO));

  /* Echo erase character as error-correcting backspace. */
  mapSetN(&module->fields, "ECHOE", valNumber(ECHOE));

  /* Echo KILL. */
  mapSetN(&module->fields, "ECHOK", valNumber(ECHOK));

  /* Echo NL. */
  mapSetN(&module->fields, "ECHONL", valNumber(ECHONL));

  /* Canonical input (erase and kill processing). */
  mapSetN(&module->fields, "ICANON", valNumber(ICANON));

  /* Enable extended input character processing. */
  mapSetN(&module->fields, "IEXTEN", valNumber(IEXTEN));

  /* Enable signals. */
  mapSetN(&module->fields, "ISIG", valNumber(ISIG));

  /* Disable flush after interrupt or quit. */
  mapSetN(&module->fields, "NOFLSH", valNumber(NOFLSH));

  /* Send SIGTTOU for background output. */
  mapSetN(&module->fields, "TOSTOP", valNumber(TOSTOP));

#endif

  return STATUS_OK;
}

static CFunction func = {impl, "termios", 1};

void addNativeModuleTermios(void) {
  addNativeModule(&func);
}

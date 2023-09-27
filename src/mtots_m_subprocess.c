#include "mtots_m_subprocess.h"

#include "mtots_vm.h"

#ifdef MTOTS_USE_POSIX
#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#define MAX_RUN_ARGC 127

#define isCompletedProcess(value) (getNativeObjectDescriptor(value) == &descriptorCompletedProcess)

typedef struct ObjCompletedProcess {
  ObjNative obj;
  int returncode;
} ObjCompletedProcess;

static ObjCompletedProcess *completedProcess;
static String *stringReturncode;

static NativeObjectDescriptor descriptorCompletedProcess = {
    nopBlacken,
    nopFree,
    sizeof(ObjCompletedProcess),
    "CompletedProcess",
};

static ObjCompletedProcess *asCompletedProcess(Value value) {
  if (!isCompletedProcess(value)) {
    panic("Expected CompletedProcess but got %s", getKindName(value));
  }
  return (ObjCompletedProcess *)AS_OBJ_UNSAFE(value);
}

static Status implCompletedProcessGetattr(i16 argc, Value *argv, Value *out) {
  ObjCompletedProcess *cp = asCompletedProcess(argv[-1]);
  String *name = asString(argv[0]);
  if (name == stringReturncode) {
    *out = NUMBER_VAL(cp->returncode);
  } else {
    runtimeError("Field '%s' not found on %s", name->chars, getKindName(argv[-1]));
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static CFunction funcCompletedProcessGetattr = {
    implCompletedProcessGetattr,
    "__getattr__",
    1,
};

static Status implRun(i16 argc, Value *argv, Value *out) {
#if MTOTS_USE_POSIX
  /*
   * https://stackoverflow.com/questions/17196877/subprocess-popen-python-in-c
   */
  const char *args[MAX_RUN_ARGC + 1] = {NULL};
  ObjList *argsList = asList(argv[0]);
  size_t i;
  pid_t child, p;
  int status;

  if (argsList->length == 0) {
    runtimeError(
        "subprocess.run()'s first argument requires at least one "
        "value for the name of the executable to run");
    return STATUS_ERROR;
  }

  if (argsList->length > MAX_RUN_ARGC) {
    runtimeError("Too many args in subprocess.run() (max = %lu, got %lu)",
                 (unsigned long)MAX_RUN_ARGC,
                 (unsigned long)argsList->length);
    return STATUS_ERROR;
  }

  for (i = 0; i < argsList->length; i++) {
    args[i] = asString(argsList->buffer[i])->chars;
  }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif
  if (0 != posix_spawnp(&child, args[0], NULL, NULL, (char *const *)args, NULL)) {
    runtimeError("posix_spawnp failed");
    return STATUS_ERROR;
  }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  /* We are at the parent process */
  do {
    status = 0;
    p = waitpid(child, &status, 0);
    if (p == (pid_t)-1 && errno != EINTR) {
      break; /* error */
    }
  } while (p != child);

  if (p != child) {
    /* Child process was lost
     * TODO: understand what this means */
    runtimeError("Child process was lost");
    return STATUS_ERROR;
  } else if (WIFEXITED(status)) {
    completedProcess->returncode = status;
  } else if (WIFSIGNALED(status)) {
    /* Child process died from a terminating signal */
    runtimeError("Child process died from signal");
    return STATUS_ERROR;
  } else {
    /* Child process died from unknown causes
     * TODO: figure out what this means */
    runtimeError("Child process died from unknown causes");
    return STATUS_ERROR;
  }

  *out = OBJ_VAL_EXPLICIT((Obj *)completedProcess);

  return STATUS_OK;
#else
  runtimeError("subprocess.run() is not supported on this platform");
  return STATUS_ERROR;
#endif
}

static const char *argsRun[] = {
    "args",
    "check",
};

static CFunction funcRun = {
    implRun,
    "run",
    1,
    sizeof(argsRun) / sizeof(argsRun[0]),
    argsRun,
};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcRun,
      NULL,
  };
  CFunction *completedProcessStaticMethods[] = {NULL};
  CFunction *completedProcessMethods[] = {
      &funcCompletedProcessGetattr,
      NULL,
  };

  moduleAddFunctions(module, functions);

  newNativeClass(
      module,
      &descriptorCompletedProcess,
      completedProcessMethods,
      completedProcessStaticMethods);

  completedProcess = NEW_NATIVE(ObjCompletedProcess, &descriptorCompletedProcess);
  moduleRetain(module, OBJ_VAL_EXPLICIT((Obj *)(completedProcess)));

  moduleRetain(module, STRING_VAL(stringReturncode = internCString("returncode")));
  return STATUS_OK;
}

static CFunction func = {impl, "subprocess", 1};

void addNativeModuleSubprocess(void) {
  addNativeModule(&func);
}

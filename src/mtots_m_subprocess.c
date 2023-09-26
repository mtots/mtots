#include "mtots_m_subprocess.h"

#include "mtots_vm.h"

#ifdef MTOTS_USE_POSIX
#include <errno.h>
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
  const char *execName;
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

  execName = asString(argsList->buffer[0])->chars;
  for (i = 1; i < argsList->length; i++) {
    args[i - 1] = asString(argsList->buffer[i])->chars;
  }

  child = fork();
  if (child == (pid_t)-1) {
    /* Cannot fork() for some reason */
    runtimeError("subprocess.run() failed to fork()");
    return STATUS_ERROR;
  }

  if (!child) {
    /* This is the child process */
    /* NOTE about const correctness:
     * https://stackoverflow.com/questions/190184/execv-and-const-ness
     * https://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html
     *
     * TBH, I think it sucks, but the standard seems to suggest
     * this is ok.
     */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif
    execvp(execName, (char *const *)args);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

    /* This is only reached if the exec failed */
    exit(127);
  }

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

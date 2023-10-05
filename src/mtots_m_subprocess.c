#include "mtots_m_subprocess.h"

#include <string.h>

#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#define RETURNCODE_PENDING (-1) /* subprocess still running */
#define RETURNCODE_MISSING (-2) /* finished, but no clear returncode */

#define MAX_RUN_ARGC 127

#define isPopen(value) (getNativeObjectDescriptor(value) == &descriptorPopen)
#define isCompletedProcess(value) (getNativeObjectDescriptor(value) == &descriptorCompletedProcess)

typedef struct Popen {
#if MTOTS_IS_POSIX
  pid_t pid;
  FILE *stdoutFile, *stderrFile;
  int stdoutfd[2];
  int stderrfd[2];
  int status;
  int returncode;
  ubool check;
  ubool captureOutput;
#else
  int unused;
#endif
} Popen;

static Status listToArgs(ObjList *argsList, const char **args) {
  size_t i;
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

  return STATUS_OK;
}

static Status popenInitFileActions(Popen *proc, posix_spawn_file_actions_t *actions) {
  if (proc->captureOutput) {
    if (0 != pipe(proc->stdoutfd)) {
      runtimeError("pipe(stdoutfd) failed");
      return STATUS_ERROR;
    }

    if (0 != pipe(proc->stderrfd)) {
      runtimeError("pipe(stderrfd) failed");
      return STATUS_ERROR;
    }

    if (0 != posix_spawn_file_actions_init(actions)) {
      runtimeError("posix_spawn_file_actions_init failed");
      return STATUS_ERROR;
    }

    /* Close the stdout read end */
    if (0 != posix_spawn_file_actions_addclose(actions, proc->stdoutfd[0])) {
      posix_spawn_file_actions_destroy(actions);
      runtimeError("posix_spawn_file_actions_addclose(.., stdoutfd[0]) failed");
      return STATUS_ERROR;
    }

    /* Map the write end to stdout */
    if (0 != posix_spawn_file_actions_adddup2(actions, proc->stdoutfd[1], STDOUT_FILENO)) {
      posix_spawn_file_actions_destroy(actions);
      runtimeError("posix_spawn_file_actions_adddup2(.., stdoutfd[1]) failed");
      return STATUS_ERROR;
    }

    /* Close the stderr read end */
    if (0 != posix_spawn_file_actions_addclose(actions, proc->stderrfd[0])) {
      posix_spawn_file_actions_destroy(actions);
      runtimeError("posix_spawn_file_actions_addclose(.., stderrfd[0]) failed");
      return STATUS_ERROR;
    }

    /* Map the write end to stdout */
    if (0 != posix_spawn_file_actions_adddup2(actions, proc->stderrfd[1],
                                              STDERR_FILENO)) {
      posix_spawn_file_actions_destroy(actions);
      runtimeError("posix_spawn_file_actions_adddup2(.., stderrfd[1]) failed");
      return STATUS_ERROR;
    }
  }

  return STATUS_OK;
}

static Status popenStart(Popen *proc, const char **args) {
  posix_spawn_file_actions_t actions;
  if (!popenInitFileActions(proc, &actions)) {
    return STATUS_ERROR;
  }

  proc->returncode = RETURNCODE_PENDING;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif
  if (0 != posix_spawnp(
               &proc->pid, args[0],
               proc->captureOutput ? &actions : NULL, NULL,
               (char *const *)args, NULL)) {
    posix_spawn_file_actions_destroy(&actions);
    runtimeError("posix_spawnp failed");
    return STATUS_ERROR;
  }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  if (proc->captureOutput) {
    posix_spawn_file_actions_destroy(&actions);

    /* Close the stdout write end */
    close(proc->stdoutfd[1]);

    /* Store the stdout read end */
    proc->stdoutFile = fdopen(proc->stdoutfd[0], "rb");

    /* Close the stderr write end */
    close(proc->stderrfd[1]);

    /* Store the stderr read end */
    proc->stderrFile = fdopen(proc->stderrfd[0], "rb");
  }

  return STATUS_OK;
}

static Status popenWait(
    Popen *proc,
    int *returncode,
    String **stdoutString,
    String **stderrString) {
  pid_t p;

  do {
    proc->status = 0;
    p = waitpid(proc->pid, &proc->status, 0);
    if (p == (pid_t)-1 && errno != EINTR) {
      break; /* error */
    }
  } while (p != proc->pid);

  *returncode = proc->returncode = RETURNCODE_MISSING;

  /* TODO: close stdoutFile and stderrFile on error */

  if (p != proc->pid) {
    /* Child process was lost
     * TODO: understand what this means */
    runtimeError("Child process was lost");
    return STATUS_ERROR;
  } else if (WIFEXITED(proc->status)) {
    *returncode = proc->returncode = WEXITSTATUS(proc->status);
  } else if (WIFSIGNALED(proc->status)) {
    /* Child process died from a terminating signal */
    if (proc->check) {
      runtimeError("Child process killed by signal %d",
                   (int)WTERMSIG(proc->status));
      return STATUS_ERROR;
    } else {
      *returncode = proc->returncode = -(int)WTERMSIG(proc->status);
    }
  } else {
    /* Child process died from unknown causes
     * TODO: figure out what this means */
    runtimeError(
        "Child process died from unknown causes "
        "(status = %d, errno = %d)",
        proc->status, (int)errno);
    return STATUS_ERROR;
  }

  if (proc->captureOutput) {
    size_t size = 0, capacity = 0;
    char *buffer = NULL;

    do {
      capacity = capacity == 0 ? 512 : capacity * 2;
      buffer = (char *)realloc(buffer, capacity);
      size += fread(buffer + size, 1, capacity - size, proc->stdoutFile);
    } while (size == capacity && !feof(proc->stdoutFile) && !ferror(proc->stdoutFile));

    *stdoutString = internString(buffer, size);

    size = 0;
    do {
      capacity = capacity == 0 ? 512
                 : size == 0   ? capacity
                               : capacity * 2;
      buffer = (char *)realloc(buffer, capacity);
      size += fread(buffer + size, 1, capacity - size, proc->stderrFile);
    } while (size == capacity && !feof(proc->stderrFile) && !ferror(proc->stderrFile));

    *stderrString = internString(buffer, size);

    free(buffer);
  } else {
    *stdoutString = vm.emptyString;
    *stderrString = vm.emptyString;
  }

  if (proc->check && proc->status != 0) {
    runtimeError("subprocess returned with non-zero exit code %d", proc->status);
    return STATUS_ERROR;
  }

  return STATUS_OK;
}

typedef struct ObjPopen {
  ObjNative obj;
  Popen handle;
} ObjPopen;

typedef struct ObjCompletedProcess {
  ObjNative obj;
  int returncode;
  String *stdoutString;
  String *stderrString;
} ObjCompletedProcess;

static ObjCompletedProcess *completedProcess;
static String *stringReturncode;
static String *stringStdout;
static String *stringStderr;

static NativeObjectDescriptor descriptorPopen = {
    nopBlacken,
    nopFree,
    sizeof(ObjPopen),
    "Popen",
};

static void blackenCompletedProcess(ObjNative *n) {
  ObjCompletedProcess *proc = (ObjCompletedProcess *)n;
  markString(proc->stdoutString);
  markString(proc->stderrString);
}

static NativeObjectDescriptor descriptorCompletedProcess = {
    blackenCompletedProcess,
    nopFree,
    sizeof(ObjCompletedProcess),
    "CompletedProcess",
};

static Value valPopen(ObjPopen *proc) {
  return valObjExplicit((Obj *)proc);
}

static ObjPopen *asPopen(Value value) {
  if (!isPopen(value)) {
    panic("Expected Popen but got %s", getKindName(value));
  }
  return (ObjPopen *)AS_OBJ_UNSAFE(value);
}

static ObjCompletedProcess *asCompletedProcess(Value value) {
  if (!isCompletedProcess(value)) {
    panic("Expected CompletedProcess but got %s", getKindName(value));
  }
  return (ObjCompletedProcess *)AS_OBJ_UNSAFE(value);
}

static ObjPopen *newPopen(void) {
  ObjPopen *proc = NEW_NATIVE(ObjPopen, &descriptorPopen);
  memset(&proc->handle, 0, sizeof(proc->handle));
  proc->handle.returncode = RETURNCODE_PENDING;
  return proc;
}

static Status implPopenStaticCall(i16 argc, Value *argv, Value *out) {
  ObjPopen *proc = newPopen();
  const char *args[MAX_RUN_ARGC + 1] = {NULL};
  ObjList *argsList = asList(argv[0]);
  proc->handle.check = argc > 1 && !isNil(argv[1]) ? asBool(argv[1]) : UFALSE;
  proc->handle.captureOutput = argc > 2 && !isNil(argv[2]) ? asBool(argv[2]) : UFALSE;
  if (!listToArgs(argsList, args)) {
    return STATUS_ERROR;
  }
  if (!popenStart(&proc->handle, args)) {
    return STATUS_ERROR;
  }
  *out = valPopen(proc);
  return STATUS_OK;
}

static const char *argsPopenStaticCall[] = {
    "args",
    "check",
    "captureOutput",
    NULL,
};

static CFunction funcPopenStaticCall = {
    implPopenStaticCall,
    "__call__",
    1,
    (sizeof(argsPopenStaticCall) / sizeof(argsPopenStaticCall[0])) - 1,
    argsPopenStaticCall,
};

static Status implPopenCommunicate(i16 argc, Value *argv, Value *out) {
  ObjPopen *proc = asPopen(argv[-1]);
  ObjList *pair = newList(2);
  String *stdoutString, *stderrString;
  int returncode;
  if (!popenWait(&proc->handle, &returncode, &stdoutString, &stderrString)) {
    return STATUS_ERROR;
  }
  pair->buffer[0] = valString(stdoutString);
  pair->buffer[1] = valString(stderrString);
  *out = valList(pair);
  return STATUS_OK;
}

static CFunction funcPopenCommunicate = {
    implPopenCommunicate,
    "communicate",
};

static Status implCompletedProcessGetattr(i16 argc, Value *argv, Value *out) {
  ObjCompletedProcess *cp = asCompletedProcess(argv[-1]);
  String *name = asString(argv[0]);
  if (name == stringReturncode) {
    *out = valNumber(cp->returncode);
  } else if (name == stringStdout) {
    *out = valString(cp->stdoutString);
  } else if (name == stringStderr) {
    *out = valString(cp->stderrString);
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
#if MTOTS_IS_POSIX
  Popen proc = {0};
  const char *args[MAX_RUN_ARGC + 1] = {NULL};
  ObjList *argsList = asList(argv[0]);
  proc.check = argc > 1 && !isNil(argv[1]) ? asBool(argv[1]) : UFALSE;
  proc.captureOutput = argc > 2 && !isNil(argv[2]) ? asBool(argv[2]) : UFALSE;

  if (!listToArgs(argsList, args)) {
    return STATUS_ERROR;
  }

  if (!popenStart(&proc, args)) {
    return STATUS_ERROR;
  }

  if (!popenWait(
          &proc,
          &completedProcess->returncode,
          &completedProcess->stdoutString,
          &completedProcess->stderrString)) {
    return STATUS_ERROR;
  }

  *out = valObjExplicit((Obj *)completedProcess);

  return STATUS_OK;
#else
  runtimeError("subprocess.run() is not supported on this platform");
  return STATUS_ERROR;
#endif
}

static const char *argsRun[] = {
    "args",
    "check",
    "captureOutput",
    NULL,
};

static CFunction funcRun = {
    implRun,
    "run",
    1,
    (sizeof(argsRun) / sizeof(argsRun[0])) - 1,
    argsRun,
};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcRun,
      NULL,
  };
  CFunction *popenStaticMethods[] = {
      &funcPopenStaticCall,
      NULL,
  };
  CFunction *popenMethods[] = {
      &funcPopenCommunicate,
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

  newNativeClass(
      module,
      &descriptorPopen,
      popenMethods,
      popenStaticMethods);

  completedProcess = NEW_NATIVE(ObjCompletedProcess, &descriptorCompletedProcess);
  completedProcess->stderrString = vm.emptyString;
  completedProcess->stdoutString = vm.emptyString;
  moduleRetain(module, valObjExplicit((Obj *)(completedProcess)));

  moduleRetain(module, valString(stringReturncode = internCString("returncode")));
  moduleRetain(module, valString(stringStdout = internCString("stdout")));
  moduleRetain(module, valString(stringStderr = internCString("stderr")));
  return STATUS_OK;
}

static CFunction func = {impl, "subprocess", 1};

void addNativeModuleSubprocess(void) {
  addNativeModule(&func);
}

#include "mtots_m_subprocess.h"

#include "mtots_vm.h"

#ifdef MTOTS_IS_POSIX
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
  String *stdoutString;
  String *stderrString;
} ObjCompletedProcess;

static ObjCompletedProcess *completedProcess;
static String *stringReturncode;
static String *stringStdout;
static String *stringStderr;

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
  } else if (name == stringStdout) {
    *out = STRING_VAL(cp->stdoutString);
  } else if (name == stringStderr) {
    *out = STRING_VAL(cp->stderrString);
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
  /*
   * https://stackoverflow.com/questions/17196877/subprocess-popen-python-in-c
   */
  const char *args[MAX_RUN_ARGC + 1] = {NULL};
  ObjList *argsList = asList(argv[0]);
  ubool check = argc > 1 && !isNil(argv[1]) ? asBool(argv[1]) : UFALSE;
  ubool captureOutput = argc > 2 && !isNil(argv[2]) ? asBool(argv[2]) : UFALSE;
  size_t i;
  pid_t child, p;
  posix_spawn_file_actions_t actions;
  int status;
  int stdoutfd[2];
  int stderrfd[2];
  FILE *stdoutFile, *stderrFile;

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

  if (captureOutput) {
    if (0 != pipe(stdoutfd)) {
      runtimeError("pipe(stdoutfd) failed");
      return STATUS_ERROR;
    }

    if (0 != pipe(stderrfd)) {
      runtimeError("pipe(stderrfd) failed");
      return STATUS_ERROR;
    }

    if (0 != posix_spawn_file_actions_init(&actions)) {
      runtimeError("posix_spawn_file_actions_init failed");
      return STATUS_ERROR;
    }

    /* Close the stdout read end */
    if (0 != posix_spawn_file_actions_addclose(&actions, stdoutfd[0])) {
      posix_spawn_file_actions_destroy(&actions);
      runtimeError("posix_spawn_file_actions_addclose(.., stdoutfd[0]) failed");
      return STATUS_ERROR;
    }

    /* Map the write end to stdout */
    if (0 != posix_spawn_file_actions_adddup2(&actions, stdoutfd[1], STDOUT_FILENO)) {
      posix_spawn_file_actions_destroy(&actions);
      runtimeError("posix_spawn_file_actions_adddup2(.., stdoutfd[1]) failed");
      return STATUS_ERROR;
    }

    /* Close the stderr read end */
    if (0 != posix_spawn_file_actions_addclose(&actions, stderrfd[0])) {
      posix_spawn_file_actions_destroy(&actions);
      runtimeError("posix_spawn_file_actions_addclose(.., stderrfd[0]) failed");
      return STATUS_ERROR;
    }

    /* Map the write end to stdout */
    if (0 != posix_spawn_file_actions_adddup2(&actions, stderrfd[1],
                                              STDERR_FILENO)) {
      posix_spawn_file_actions_destroy(&actions);
      runtimeError("posix_spawn_file_actions_adddup2(.., stderrfd[1]) failed");
      return STATUS_ERROR;
    }
  }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif
  if (0 != posix_spawnp(
               &child, args[0],
               captureOutput ? &actions : NULL, NULL,
               (char *const *)args, NULL)) {
    posix_spawn_file_actions_destroy(&actions);
    runtimeError("posix_spawnp failed");
    return STATUS_ERROR;
  }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  if (captureOutput) {
    posix_spawn_file_actions_destroy(&actions);

    /* Close the stdout write end */
    close(stdoutfd[1]);

    /* Store the stdout read end */
    stdoutFile = fdopen(stdoutfd[0], "rb");

    /* Close the stderr write end */
    close(stderrfd[1]);

    /* Store the stderr read end */
    stderrFile = fdopen(stderrfd[0], "rb");
  }

  /* We are at the parent process */
  do {
    status = 0;
    p = waitpid(child, &status, 0);
    if (p == (pid_t)-1 && errno != EINTR) {
      break; /* error */
    }
  } while (p != child);

  /* TODO: close stdoutFile and stderrFile on error */

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

  if (captureOutput) {
    size_t size = 0, capacity = 0;
    char *buffer = NULL;

    do {
      capacity = capacity == 0 ? 512 : capacity * 2;
      buffer = (char *)realloc(buffer, capacity);
      size += fread(buffer + size, 1, capacity - size, stdoutFile);
    } while (size == capacity && !feof(stdoutFile) && !ferror(stdoutFile));

    completedProcess->stdoutString = internString(buffer, size);

    size = 0;
    do {
      capacity = capacity == 0 ? 512
                 : size == 0   ? capacity
                               : capacity * 2;
      buffer = (char *)realloc(buffer, capacity);
      size += fread(buffer + size, 1, capacity - size, stderrFile);
    } while (size == capacity && !feof(stderrFile) && !ferror(stderrFile));

    completedProcess->stderrString = internString(buffer, size);

    free(buffer);
  } else {
    completedProcess->stdoutString = vm.emptyString;
    completedProcess->stderrString = vm.emptyString;
  }

  if (check && status != 0) {
    runtimeError("subprocess returned with non-zero exit code %d", status);
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
  completedProcess->stderrString = vm.emptyString;
  completedProcess->stdoutString = vm.emptyString;
  moduleRetain(module, OBJ_VAL_EXPLICIT((Obj *)(completedProcess)));

  moduleRetain(module, STRING_VAL(stringReturncode = internCString("returncode")));
  moduleRetain(module, STRING_VAL(stringStdout = internCString("stdout")));
  moduleRetain(module, STRING_VAL(stringStderr = internCString("stderr")));
  return STATUS_OK;
}

static CFunction func = {impl, "subprocess", 1};

void addNativeModuleSubprocess(void) {
  addNativeModule(&func);
}

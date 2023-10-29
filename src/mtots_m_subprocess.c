#include "mtots_m_subprocess.h"

#include <string.h>

#include "mtots_util_proc.h"
#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#define MTOTS_PIPE (-1)

#define RETURNCODE_PENDING (-1) /* subprocess still running */
#define RETURNCODE_MISSING (-2) /* finished, but no clear returncode */

#define MAX_RUN_ARGC 127

#define isPopen(value) (getNativeObjectDescriptor(value) == &descriptorPopen)
#define isCompletedProcess(value) (getNativeObjectDescriptor(value) == &descriptorCompletedProcess)

static void initProcArgs(MTOTSProc *proc, ObjList *argsList) {
  size_t i;
  if (argsList->length == 0) {
    panic("subprocess args requires at least one argument but got zero");
  }
  MTOTSProcFreeArgs(proc);
  proc->argv = (char **)malloc(sizeof(char *) * (argsList->length + 1));
  for (i = 0; i < argsList->length; i++) {
    String *string = asString(argsList->buffer[i]);
    proc->argv[i] = (char *)malloc(string->byteLength + 1);
    memcpy(proc->argv[i], string->chars, string->byteLength);
    proc->argv[i][string->byteLength] = '\0';
  }
  proc->argv[argsList->length] = NULL;
}

static int asFileDescriptor(i16 argc, Value *argv, i16 i) {
  return i < argc && !isNil(argv[i]) ? asInt(argv[i]) : MTOTS_PROC_INHERIT;
}

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
  String *input;
  ubool captureOutput;
  MTOTSProc proc;
  ObjList *argsList = asList(argv[0]);
  MTOTSProcInit(&proc);
  initProcArgs(&proc, argsList);

  proc.checkReturnCode = argc > 1 && !isNil(argv[1]) ? asBool(argv[1]) : UFALSE;
  proc.stdinFD = asFileDescriptor(argc, argv, 2);
  proc.stdoutFD = asFileDescriptor(argc, argv, 3);
  proc.stderrFD = asFileDescriptor(argc, argv, 4);

  input = argc > 5 && !isNil(argv[5]) ? asString(argv[5]) : NULL;
  captureOutput = argc > 6 && !isNil(argv[6]) ? asBool(argv[6]) : UFALSE;

  if (input) {
    if (proc.stdinFD != MTOTS_PROC_INHERIT) {
      panic(
          "subprocess.run(): stdin argument cannot be provided "
          "if input is also provided");
    }
    proc.stdinFD = MTOTS_PROC_PIPE;
  }

  if (captureOutput) {
    if (proc.stdoutFD != MTOTS_PROC_INHERIT || proc.stderrFD != MTOTS_PROC_INHERIT) {
      panic(
          "subprocess.run(): a stdout or stderr argument cannot be provided"
          "if captureOutput is also set");
    }
    proc.stdoutFD = MTOTS_PROC_PIPE;
    proc.stderrFD = MTOTS_PROC_PIPE;
  }

  if (!MTOTSProcStart(&proc)) {
    MTOTSProcFree(&proc);
    return STATUS_ERROR;
  }

  /* TODO: pipe input to stdin */
  if (!MTOTSProcWait(&proc)) {
    MTOTSProcFree(&proc);
    return STATUS_ERROR;
  }

  completedProcess->returncode = proc.returncode;
  completedProcess->stdoutString = bufferToString(&proc.outputData[0]);
  completedProcess->stderrString = bufferToString(&proc.outputData[1]);

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
    "stdin",
    "stdout",
    "stderr",
    "input",
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
  completedProcess->stderrString = vm.cs->empty;
  completedProcess->stdoutString = vm.cs->empty;
  moduleRetain(module, valObjExplicit((Obj *)(completedProcess)));

  moduleRetain(module, valString(stringReturncode = internCString("returncode")));
  moduleRetain(module, valString(stringStdout = internCString("stdout")));
  moduleRetain(module, valString(stringStderr = internCString("stderr")));

  mapSetN(&module->fields, "PIPE", valNumber(MTOTS_PIPE));

  return STATUS_OK;
}

static CFunction func = {impl, "subprocess", 1};

void addNativeModuleSubprocess(void) {
  addNativeModule(&func);
}

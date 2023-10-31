#include "mtots_m_subprocess.h"

#include <string.h>

#include "mtots.h"
#include "mtots_util_proc.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <spawn.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#define RETURNCODE_PENDING (-1) /* subprocess still running */
#define RETURNCODE_MISSING (-2) /* finished, but no clear returncode */

#define MAX_RUN_ARGC 127

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

typedef struct ObjPopen {
  ObjNative obj;
  MTOTSProc handle;
} ObjPopen;

static void freePopen(ObjNative *n) {
  ObjPopen *po = (ObjPopen *)n;
  MTOTSProcFree(&po->handle);
}

WRAP_C_TYPE_EX(Popen, MTOTSProc, static, nopBlacken, freePopen)

static Status implPopenStaticCall(i16 argc, Value *argv, Value *out) {
  ObjPopen *po = allocPopen();
  MTOTSProc *proc = &po->handle;
  ObjList *argsList = asList(argv[0]);
  MTOTSProcInit(proc);
  initProcArgs(proc, argsList);

  proc->stdinFD = asFileDescriptor(argc, argv, 1);
  proc->stdoutFD = asFileDescriptor(argc, argv, 2);
  proc->stderrFD = asFileDescriptor(argc, argv, 3);

  if (!MTOTSProcStart(proc)) {
    MTOTSProcFree(proc);
    return STATUS_ERROR;
  }

  *out = valPopen(po);

  return STATUS_OK;
}

static const char *argsPopenStaticCall[] = {
    "args",
    "stdin",
    "stdout",
    "stderr",
    NULL,
};

static CFunction funcPopenStaticCall = {
    implPopenStaticCall,
    "__call__",
    1,
    (sizeof(argsPopenStaticCall) / sizeof(argsPopenStaticCall[0])) - 1,
    argsPopenStaticCall,
};
WRAP_C_FUNCTION_EX(wait, PopenWait, 0, 0, {
  ObjPopen *po = asPopen(argv[-1]);
  MTOTSProc *proc = &po->handle;
  if (!MTOTSProcWait(proc)) {
    return STATUS_ERROR;
  }
  *out = valNumber(proc->returncode);
  return STATUS_OK;
})
WRAP_C_FUNCTION_EX(communicate, PopenCommunicate, 0, 3, {
  ObjPopen *po = asPopen(argv[-1]);
  MTOTSProc *proc = &po->handle;
  ByteSlice inputSliceBuffer;
  ByteSlice *inputSlice = NULL;
  Buffer *stdoutData = NULL;
  Buffer *stderrData = NULL;
  if (argc > 0 && !isNil(argv[0])) {
    inputSlice = &inputSliceBuffer;
    if (isString(argv[0])) {
      String *string = asString(argv[0]);
      inputSlice->start = (const u8 *)string->chars;
      inputSlice->end = inputSlice->start + string->byteLength;
    } else {
      ObjBuffer *buffer = asBuffer(argv[0]);
      inputSlice->start = buffer->handle.data;
    }
  }
  if (argc > 1 && !isNil(argv[1])) {
    stdoutData = &asBuffer(argv[1])->handle;
  }
  if (argc > 2 && !isNil(argv[2])) {
    stderrData = &asBuffer(argv[2])->handle;
  }
  return MTOTSProcCommunicate(proc, inputSlice, stdoutData, stderrData);
})
DEFINE_FIELD_GETTER(Popen, returncode, valNumber(owner->handle.returncode))
DEFINE_FIELD_GETTER(Popen, stdinPipe, valNumber(owner->handle.stdinPipe[1]))
DEFINE_FIELD_GETTER(Popen, stdoutPipe, valNumber(owner->handle.stdoutPipe[0]))
DEFINE_FIELD_GETTER(Popen, stderrPipe, valNumber(owner->handle.stderrPipe[0]))

static CFunction *PopenStaticMethods[] = {
    &funcPopenStaticCall,
    NULL,
};
static CFunction *PopenMethods[] = {
    &funcPopenWait,
    &funcPopenCommunicate,
    &funcPopen_getreturncode,
    &funcPopen_getstdinPipe,
    &funcPopen_getstdoutPipe,
    &funcPopen_getstderrPipe,
    NULL,
};

typedef struct CompletedProcess {
  int returncode;
  String *stdoutString;
  String *stderrString;
} CompletedProcess;

typedef struct ObjCompletedProcess {
  ObjNative obj;
  CompletedProcess handle;
} ObjCompletedProcess;

static void blackenCompletedProcess(ObjNative *n) {
  ObjCompletedProcess *cp = (ObjCompletedProcess *)n;
  markString(cp->handle.stdoutString);
  markString(cp->handle.stderrString);
}

WRAP_C_TYPE_EX(
    CompletedProcess,
    CompletedProcess,
    static,
    blackenCompletedProcess,
    nopFree)
DEFINE_FIELD_GETTER(CompletedProcess, returncode, valNumber(owner->handle.returncode))
DEFINE_FIELD_GETTER(CompletedProcess, stdout, valString(owner->handle.stdoutString))
DEFINE_FIELD_GETTER(CompletedProcess, stderr, valString(owner->handle.stderrString))
static CFunction *CompletedProcessMethods[] = {
    &funcCompletedProcess_getreturncode,
    &funcCompletedProcess_getstdout,
    &funcCompletedProcess_getstderr,
    NULL,
};
static CFunction *CompletedProcessStaticMethods[] = {
    NULL,
};

static ObjCompletedProcess *completedProcess;

static Status implRun(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  String *input;
  ubool captureOutput;
  MTOTSProc proc;
  ByteSlice inputSlice;
  ObjList *argsList = asList(argv[0]);
  Buffer stdoutData, stderrData;
  MTOTSProcInit(&proc);
  initProcArgs(&proc, argsList);
  initBuffer(&stdoutData);
  initBuffer(&stderrData);

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
    inputSlice.start = (const u8 *)input->chars;
    inputSlice.end = inputSlice.start + input->byteLength;
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
  if (!MTOTSProcCommunicate(&proc, input ? &inputSlice : NULL, &stdoutData, &stderrData)) {
    MTOTSProcFree(&proc);
    return STATUS_ERROR;
  }

  completedProcess->handle.returncode = proc.returncode;
  completedProcess->handle.stdoutString = bufferToString(&stdoutData);
  completedProcess->handle.stderrString = bufferToString(&stderrData);

  *out = valCompletedProcess(completedProcess);

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
  CommonStrings *cs = getCommonStrings();

  moduleAddFunctions(module, functions);

  ADD_TYPE_TO_MODULE(Popen);
  ADD_TYPE_TO_MODULE(CompletedProcess);

  completedProcess = allocCompletedProcess();
  completedProcess->handle.returncode = -1;
  completedProcess->handle.stderrString = cs->empty;
  completedProcess->handle.stdoutString = cs->empty;
  moduleRetain(module, valObjExplicit((Obj *)(completedProcess)));

  mapSetN(&module->fields, "PIPE", valNumber(MTOTS_PROC_PIPE));
  mapSetN(&module->fields, "STDOUT", valNumber(MTOTS_PROC_STDOUT));
  mapSetN(&module->fields, "DEVNULL", valNumber(MTOTS_PROC_DEVNULL));

  return STATUS_OK;
}

static CFunction func = {impl, "subprocess", 1};

void addNativeModuleSubprocess(void) {
  addNativeModule(&func);
}

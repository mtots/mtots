#ifndef mtots_util_proc_h
#define mtots_util_proc_h

/* Utilities for starting and interacting with subprocesses */

#include "mtots_util_buffer.h"

#if MTOTS_IS_POSIX
#include <sys/types.h>
#endif

#define MTOTS_PROC_PIPE (-1)
#define MTOTS_PROC_STDOUT (-2)
#define MTOTS_PROC_DEVNULL (-3)
#define MTOTS_PROC_INHERIT (-4)

typedef enum MTOTSProcState {
  MTOTS_PROC_NOT_STARTED, /* child process has not been spawned */
  MTOTS_PROC_STARTED,     /* child process has been spawned but not waited on */
  MTOTS_PROC_FINISHED     /* the child process has been waited on */
} MTOTSProcState;

/* Subprocess */
typedef struct MTOTSProc {
  MTOTSProcState state;
  size_t argc;
  char **argv; /* should not be set directly - use MTOTSProcSetArgs() */
#if MTOTS_IS_POSIX
  int stdinFD;
  int stdoutFD;
  int stderrFD;
  int stdinPipe[2];
  int stdoutPipe[2];
  int stderrPipe[2];
  pid_t pid;
  int returncode;
  ubool checkReturnCode;
#endif

  /* any stdout data piped while MTOTSProcWait runs will be written here */
  Buffer stdoutData;

  /* any stderr data piped while MTOTSProcWait runs will be written here */
  Buffer stderrData;

} MTOTSProc;

void MTOTSProcInit(MTOTSProc *proc);
void MTOTSProcFree(MTOTSProc *proc);
void MTOTSProcFreeArgs(MTOTSProc *proc);
void MTOTSProcSetArgs(MTOTSProc *proc, const char **argv, size_t argc);
Status MTOTSProcStart(MTOTSProc *proc);
Status MTOTSProcWait(MTOTSProc *proc, ByteSlice *inputSlice);

#endif /*mtots_util_proc_h*/

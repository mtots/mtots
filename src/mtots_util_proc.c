#include "mtots_util_proc.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_util_error.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <spawn.h>
#include <unistd.h>

#include "mtots_util_fd.h"
#endif

static void freeArgv(MTOTSProc *proc) {
  if (proc->argv) {
    size_t i;
    for (i = 0; i < proc->argc; i++) {
      free(proc->argv[i]);
    }
    free(proc->argv);
  }
  proc->argv = NULL;
  proc->argc = 0;
}

void MTOTSProcInit(MTOTSProc *proc) {
  proc->state = MTOTS_PROC_NOT_STARTED;
  proc->pid = -1;
  proc->argc = 0;
  proc->argv = NULL;
  proc->stdinFD = proc->stdoutFD = proc->stderrFD = MTOTS_PROC_INHERIT;
  proc->stdinPipe[0] = proc->stdinPipe[1] = -1;
  proc->stdoutPipe[0] = proc->stdoutPipe[1] = -1;
  proc->stderrPipe[0] = proc->stderrPipe[1] = -1;
  proc->pid = -1;
  proc->returncode = -1;
  proc->checkReturnCode = UFALSE;
  initBuffer(&proc->outputData[0]);
  initBuffer(&proc->outputData[1]);
}

void MTOTSProcFree(MTOTSProc *proc) {
  /* TODO: kill process if it is running */
  freeArgv(proc);
  freeBuffer(&proc->outputData[0]);
  freeBuffer(&proc->outputData[1]);
}

void MTOTSProcSetArgs(MTOTSProc *proc, const char **argv, size_t argc) {
  size_t i;
  freeArgv(proc);
  proc->argv = (char **)malloc(sizeof(char *) * (argc + 1));
  proc->argc = argc;
  for (i = 0; i < argc; i++) {
    size_t len = strlen(argv[i]);
    proc->argv[i] = (char *)malloc(sizeof(char) * (len + 1));
    memcpy(proc->argv[i], argv[i], len);
    proc->argv[i][len] = '\0';
  }
  proc->argv[argc] = NULL;
}

#if MTOTS_IS_POSIX
static Status handleInputFileActions(
    posix_spawn_file_actions_t *fileActions,
    int fd,
    int pipeFDs[2],
    int targetFD) {
  int status;
  if (fd == MTOTS_PROC_INHERIT) {
    /* nothing to do */
    return STATUS_OK;
  }
  if (fd == MTOTS_PROC_DEVNULL) {
    /* TODO */
    runtimeError("reading from DEVNULL not yet supported");
    return STATUS_ERROR;
  }
  if (fd == MTOTS_PROC_PIPE) {
    /* PIPE input to child */
    if (pipe(pipeFDs) != 0) {
      runtimeError("pipe(): %s", strerror(errno));
      return STATUS_ERROR;
    }

    /* Close the write end in the child process */
    if ((status = posix_spawn_file_actions_addclose(fileActions, pipeFDs[1])) != 0) {
      runtimeError("posix_spawn_file_actions_addclose(): %s", strerror(status));
      return STATUS_ERROR;
    }

    /* Map the read end to the target FD */
    if ((status = posix_spawn_file_actions_adddup2(
             fileActions, pipeFDs[0], targetFD)) != 0) {
      runtimeError("posix_spawn_file_actions_adddup2(): %s", strerror(status));
      return STATUS_ERROR;
    }

    /* Close the pipe read end in the child process */
    if ((status = posix_spawn_file_actions_addclose(fileActions, pipeFDs[0])) != 0) {
      runtimeError("posix_spawn_file_actions_addclose(): %s", strerror(status));
      return STATUS_ERROR;
    }
  }
  if (fd >= 0) {
    /* Map input from a specific file descriptor */
    if ((status = posix_spawn_file_actions_adddup2(
             fileActions, fd, targetFD)) != 0) {
      runtimeError("posix_spawn_file_actions_adddup2(): %s", strerror(status));
      return STATUS_ERROR;
    }
  }
  runtimeError("Invalid MTOTSProc input fd value %d", fd);
  return STATUS_ERROR;
}
static Status handleOutputFileActions(
    posix_spawn_file_actions_t *fileActions,
    int fd,
    int pipeFDs[2],
    int targetFD) {
  int status;
  if (fd == MTOTS_PROC_INHERIT) {
    /* nothing to do */
    return STATUS_OK;
  }
  if (fd == MTOTS_PROC_DEVNULL) {
    /* TODO */
    runtimeError("writing to DEVNULL not yet supported");
    return STATUS_ERROR;
  }
  if (fd == MTOTS_PROC_STDOUT) {
    /* Redirect STDERR to STDOUT */
    if (targetFD == STDERR_FILENO) {
      /* Map the write end to the target FD */
      if ((status = posix_spawn_file_actions_adddup2(
               fileActions, STDOUT_FILENO, targetFD)) != 0) {
        runtimeError("posix_spawn_file_actions_adddup2(): %s", strerror(status));
        return STATUS_ERROR;
      }
      return STATUS_OK;
    }
    runtimeError("STDOUT fd can only be used with stderr");
    return STATUS_ERROR;
  }
  if (fd == MTOTS_PROC_PIPE) {
    /* PIPE the output from child */
    if (pipe(pipeFDs) != 0) {
      runtimeError("pipe(): %s", strerror(errno));
      return STATUS_ERROR;
    }

    /* Close the read end in the child process */
    if ((status = posix_spawn_file_actions_addclose(fileActions, pipeFDs[0])) != 0) {
      runtimeError("posix_spawn_file_actions_addclose(): %s", strerror(status));
      return STATUS_ERROR;
    }

    /* Map the write end to the target FD */
    if ((status = posix_spawn_file_actions_adddup2(
             fileActions, pipeFDs[1], targetFD)) != 0) {
      runtimeError("posix_spawn_file_actions_adddup2(): %s", strerror(status));
      return STATUS_ERROR;
    }

    /* Close the pipe write end in the child process */
    if ((status = posix_spawn_file_actions_addclose(fileActions, pipeFDs[1])) != 0) {
      runtimeError("posix_spawn_file_actions_addclose(): %s", strerror(status));
      return STATUS_ERROR;
    }

    return STATUS_OK;
  }
  if (fd >= 0) {
    /* Map output to a specific file descriptor */
    if ((status = posix_spawn_file_actions_adddup2(
             fileActions, fd, targetFD)) != 0) {
      runtimeError("posix_spawn_file_actions_adddup2(): %s", strerror(status));
      return STATUS_ERROR;
    }
  }
  runtimeError("Invalid MTOTSProc output fd value %d", fd);
  return STATUS_ERROR;
}
#endif

Status MTOTSProcStart(MTOTSProc *proc) {
#if MTOTS_IS_POSIX
  int status;
  posix_spawn_file_actions_t fileActions;
  posix_spawn_file_actions_init(&fileActions);
  if (!handleInputFileActions(
          &fileActions, proc->stdinFD, proc->stdinPipe, STDIN_FILENO) ||
      !handleOutputFileActions(
          &fileActions, proc->stdoutFD, proc->stdoutPipe, STDOUT_FILENO) ||
      !handleOutputFileActions(
          &fileActions, proc->stderrFD, proc->stderrPipe, STDERR_FILENO)) {
    posix_spawn_file_actions_destroy(&fileActions);
    return STATUS_ERROR;
  }
  if ((status = posix_spawnp(
           &proc->pid,
           proc->argv[0],
           &fileActions,
           NULL,
           proc->argv,
           NULL)) != 0) {
    posix_spawn_file_actions_destroy(&fileActions);
    runtimeError("posix_spawnp: %s", strerror(status));
    return STATUS_ERROR;
  }
  posix_spawn_file_actions_destroy(&fileActions);
  proc->state = MTOTS_PROC_STARTED;

  /* Close the unneeded pipes in the parent process */

  if (proc->stdinFD == MTOTS_PROC_PIPE) {
    close(proc->stdinPipe[0]); /* close read end */
    proc->stdinPipe[0] = -1;
  }

  if (proc->stdoutFD == MTOTS_PROC_PIPE) {
    close(proc->stdoutPipe[1]); /* close write end */
    proc->stdoutPipe[1] = -1;
  }

  if (proc->stderrFD == MTOTS_PROC_PIPE) {
    close(proc->stderrPipe[1]); /* close write end */
    proc->stderrPipe[1] = -1;
  }

  return STATUS_OK;
#else
  runtimeError("Operation not supported for platform (MTOTSProcStart)");
  return STATUS_ERROR;
#endif
}

Status MTOTSProcWait(MTOTSProc *proc) {
#if MTOTS_IS_POSIX
  int fds[2], status;
  pid_t p;
  fds[0] = proc->stdoutPipe[0];
  fds[1] = proc->stderrPipe[0];
  if (!readFromMultipleFDs(fds, proc->outputData, 2)) {
    return STATUS_ERROR;
  }

  do {
    status = 0;
    p = waitpid(proc->pid, &status, 0);
    if (p == (pid_t)-1 && errno != EINTR) {
      break; /* error */
    }
  } while (p != proc->pid);

  proc->state = MTOTS_PROC_FINISHED;
  proc->returncode = -2;

  if (p != proc->pid) {
    /* Child process was lost
     * TODO: understand what this means */
    runtimeError("Child process was lost");
    return STATUS_ERROR;
  } else if (WIFEXITED(status)) {
    proc->returncode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    /* Child process died from a terminating signal */
    if (proc->checkReturnCode) {
      runtimeError("Child process killed by signal %d",
                   (int)WTERMSIG(status));
      return STATUS_ERROR;
    } else {
      proc->returncode = -(int)WTERMSIG(status);
    }
  } else {
    /* Child process died from unknown causes
     * TODO: figure out what this means */
    runtimeError(
        "Child process died from unknown causes "
        "(status = %d, errno = %d)",
        status, (int)errno);
    return STATUS_ERROR;
  }

  if (proc->checkReturnCode && proc->returncode != 0) {
    runtimeError("subprocess returned with non-zero exit code %d", proc->returncode);
    return STATUS_ERROR;
  }

  return STATUS_OK;
#else
  runtimeError("Operation not supported for platform (MTOTSProcWait)");
  return STATUS_ERROR;
#endif
}

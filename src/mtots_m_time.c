
#include "mtots_m_time.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define NSEC_IN_SEC 1000000000

static Status implSleep(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  double sleepSec = asNumber(argv[0]);
  struct timespec req, rem;
  if (sleepSec < 0) {
    sleepSec = 0;
  }
  req.tv_sec = (time_t)sleepSec;
  req.tv_nsec = (long)((sleepSec - req.tv_sec) * NSEC_IN_SEC);
sleepAgain:
  if (nanosleep(&req, &rem) != 0) {
    if (errno == EINTR) {
      /* Behavior proposed by https://peps.python.org/pep-0475/ */
      if (!checkAndHandleSignals()) {
        return STATUS_ERROR;
      }
      req = rem;
      goto sleepAgain;
    }
    runtimeError("nanosleep(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valNumber(0);
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (sleep())");
  return STATUS_ERROR;
#endif
}

static CFunction funcSleep = {implSleep, "sleep", 1};

static Status implTime(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    runtimeError("clock_gettime(): %s", strerror(errno));
    return STATUS_ERROR;
  }
  *out = valNumber(ts.tv_sec + ((double)ts.tv_nsec) / NSEC_IN_SEC);
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (time())");
  return STATUS_ERROR;
#endif
}

static CFunction funcTime = {implTime, "time"};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcSleep,
      &funcTime,
      NULL,
  };

  moduleAddFunctions(module, functions);

  return STATUS_OK;
}

static CFunction func = {impl, "time", 1};

void addNativeModuleTime(void) {
  addNativeModule(&func);
}

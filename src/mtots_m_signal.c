#include "mtots_m_signal.h"

#include <stdlib.h>
#include <string.h>

#include "mtots_vm.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if MTOTS_IS_POSIX
static void signalHandler(int signal) {
  /* NOTE: simply storing just the latest signal instead of
   * maintaining a queue may lead to race conditions...
   * TODO: address this */
  vm.trap = UTRUE;
  vm.signal = signal;
}
#endif

static Status implNop(i16 argc, Value *argv, Value *out) {
  return STATUS_OK;
}

static CFunction funcSIG_IGN = {implNop, "SIG_IGN", 1};
static CFunction funcSIG_DFL = {implNop, "SIG_DFL", 1};

void setupDefaultMtotsSIGINTHandler(void) {
#if MTOTS_IS_POSIX
  struct sigaction action;
  action.sa_handler = signalHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  if (sigaction(SIGINT, &action, NULL) != 0) {
    panic("sigaction (for setupDefaultMtotsSIGINTHandler): %s",
          strerror(errno));
  }
  vm.signalHandlers[SIGINT] = valNil();
#endif
}

static Status implSignal(i16 argc, Value *argv, Value *out) {
#if MTOTS_IS_POSIX
  int sig = asInt(argv[0]);
  Value callback = argv[1];
  if (sig < 0 || sig >= SIGNAL_HANDLERS_COUNT) {
    runtimeError("Invalid signal (must be in [0, %d], but got %d)",
                 SIGNAL_HANDLERS_COUNT - 1, sig);
    return STATUS_ERROR;
  }
  vm.signalHandlers[sig] = callback;
  if (isCFunction(callback) && callback.as.cfunction == &funcSIG_IGN) {
    signal(sig, SIG_IGN);
  } else if (isCFunction(callback) &&
             callback.as.cfunction == &funcSIG_DFL &&
             sig != SIGINT) {
    signal(sig, SIG_DFL);
  } else {
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(sig, &action, NULL) != 0) {
      runtimeError("sigaction: %s", strerror(errno));
      return STATUS_ERROR;
    }
  }
  return STATUS_OK;
#else
  runtimeError("Unsupported platfom (signal())");
  return STATUS_ERROR;
#endif
}

static CFunction funcSignal = {implSignal, "signal", 2};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcSignal,
      NULL,
  };

  moduleAddFunctions(module, functions);

  mapSetN(&module->fields, "SIG_DFL", valCFunction(&funcSIG_DFL));
  mapSetN(&module->fields, "SIG_IGN", valCFunction(&funcSIG_IGN));

#if MTOTS_IS_POSIX
  mapSetN(&module->fields, "SIGHUP", valNumber(SIGHUP));
  mapSetN(&module->fields, "SIGINT", valNumber(SIGINT));
  mapSetN(&module->fields, "SIGQUIT", valNumber(SIGQUIT));
  mapSetN(&module->fields, "SIGILL", valNumber(SIGILL));
  mapSetN(&module->fields, "SIGABRT", valNumber(SIGABRT));
  mapSetN(&module->fields, "SIGFPE", valNumber(SIGFPE));
  mapSetN(&module->fields, "SIGKILL", valNumber(SIGKILL));
  mapSetN(&module->fields, "SIGSEGV", valNumber(SIGSEGV));
  mapSetN(&module->fields, "SIGPIPE", valNumber(SIGPIPE));
  mapSetN(&module->fields, "SIGALRM", valNumber(SIGALRM));
  mapSetN(&module->fields, "SIGTERM", valNumber(SIGTERM));
  mapSetN(&module->fields, "SIGCHLD", valNumber(SIGCHLD));
#endif

  return STATUS_OK;
}

static CFunction func = {impl, "signal", 1};

void addNativeModuleSignal(void) {
  addNativeModule(&func);
}

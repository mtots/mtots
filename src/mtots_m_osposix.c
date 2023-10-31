#include "mtots_m_osposix.h"

#include "mtots.h"

#if MTOTS_IS_POSIX

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define ADD_CONST_INT(name, value) \
  mapSetN(&module->fields, name, valNumber((value)))

static Status implOpen(i16 argc, Value *argv, Value *out) {
  const char *pathname = asString(argv[0])->chars;
  int flags = asInt(argv[1]);
  int fd;
  if (flags & O_CREAT) {
    if (argc > 2 && !isNil(argv[2])) {
      return runtimeError(
          "open with O_CREAT requires a third argument but got %d", argc);
    }
    fd = open(pathname, flags, asInt(argv[2]));
  } else {
    if (!(argc == 2 || (argc > 2 && isNil(argv[2])))) {
      return runtimeError(
          "open without O_CREAT must have exactly 2 arguments but got %d", argc);
    }
    fd = open(pathname, flags);
  }
  if (fd < 0) {
    return runtimeError("open(): %s", strerror(errno));
  }
  *out = valFileDescriptor(fd);
  return STATUS_OK;
}

static CFunction funcOpen = {implOpen, "open", 2, 3};

static Status implClose(i16 argc, Value *argv, Value *out) {
  int fd = asFileDescriptor(argv[0]);
  if (close(fd) < 0) {
    return runtimeError("close(): %s", strerror(errno));
  }
  return STATUS_OK;
}

static CFunction funcClose = {implClose, "close", 1};

static Status implRead(i16 argc, Value *argv, Value *out) {
  int fd = asFileDescriptor(argv[0]);
  Buffer *buffer = &asBuffer(argv[1])->handle;

  if (argc > 2 && !isNil(argv[2])) {
    /* number of bytes to read is explicitly specified */
    size_t nbytes = asSize(argv[2]);
    ssize_t bytesRead;
    bufferSetMinCapacity(buffer, buffer->length + nbytes);
    bytesRead = read(fd, buffer->data + buffer->length, nbytes);
    if (bytesRead < 0) {
      return runtimeError("read(): %s", strerror(errno));
    }
    buffer->length += bytesRead;
    *out = valNumber(bytesRead);
  } else {
    /* number of bytes is not specified - read till EOF */
    ssize_t bytesRead;
    size_t startBufferLength = buffer->length;
    for (;;) {
      bufferSetMinCapacity(buffer, buffer->length < 2048 ? 4096 : buffer->length * 2);
      bytesRead = read(
          fd,
          buffer->data + buffer->length,
          buffer->capacity - buffer->length);
      if (bytesRead < 0) {
        return runtimeError("read(): %s", strerror(errno));
      }
      if (bytesRead == 0) {
        break;
      }
      buffer->length += bytesRead;
    }
    *out = valNumber(buffer->length - startBufferLength);
  }
  return STATUS_OK;
}

static CFunction funcRead = {implRead, "read", 2, 3};

static Status impl(i16 argc, Value *argv, Value *out) {
  ObjModule *module = asModule(argv[0]);
  CFunction *functions[] = {
      &funcOpen,
      &funcClose,
      &funcRead,
      NULL,
  };

  moduleAddFunctions(module, functions);
  ADD_CONST_INT("O_RDONLY", O_RDONLY);
  ADD_CONST_INT("O_WRONLY", O_WRONLY);
  ADD_CONST_INT("O_RDWR", O_RDWR);
  ADD_CONST_INT("O_APPEND", O_APPEND);
  ADD_CONST_INT("O_CREAT", O_CREAT);
  ADD_CONST_INT("O_DSYNC", O_DSYNC);
  ADD_CONST_INT("O_CLOEXEC", O_CLOEXEC);
  ADD_CONST_INT("O_EXCL", O_EXCL);
  ADD_CONST_INT("O_NONBLOCK", O_NONBLOCK);
  ADD_CONST_INT("O_TRUNC", O_TRUNC);

#ifdef O_RSYNC
  ADD_CONST_INT("O_RSYNC", O_RSYNC);
#endif

#ifdef O_SYNC
  ADD_CONST_INT("O_SYNC", O_SYNC);
#endif

  return STATUS_OK;
}

#else

static Status impl(i16 argc, Value *argv, Value *out) {
  return STATUS_OK;
}
#endif

static CFunction func = {impl, "os.posix", 1};

void addNativeModuleOsPosix(void) {
  addNativeModule(&func);
}

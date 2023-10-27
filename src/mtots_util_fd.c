#include "mtots_util_fd.h"

#include <string.h>

#include "mtots_util_error.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#endif

#define MAX_NFDS 8
#define MIN_READ_SIZE 4096

Status readFromMultipleFDs(int *fds, Buffer *buffers, size_t nfds) {
#if MTOTS_IS_POSIX
  struct pollfd pfds[MAX_NFDS];
  size_t i, finished = 0;
  int pollResult;

  if (nfds > MAX_NFDS) {
    runtimeError("Too many file descriptors nfds = %lu, MAX_NFDS = %d",
                 (unsigned long)nfds, MAX_NFDS);
    return STATUS_ERROR;
  }

  for (i = 0; i < nfds; i++) {
    pfds[i].fd = fds[i];
    pfds[i].events = POLLIN;
    pfds[i].revents = 0;
    if (fds[i] < 0) {
      finished++;
    }
  }

  while (finished < nfds) {
    errno = 0;
    pollResult = poll(pfds, nfds, -1);
    if (pollResult < 0) {
      /* error */
      if (errno == EINTR) {
        /* signal/interrupt - we should retry */
        continue;
      }
      runtimeError("poll(): %s", strerror(errno));
      return STATUS_ERROR;
    } else if (pollResult == 0) {
      /* timeout. This should be impossible */
      panic("poll() timed out with infinite timeout");
    }
    /* some file descriptors are ready */
    for (i = 0; i < nfds; i++) {
      struct pollfd *pfd = &pfds[i];
      if (pfd->revents == 0) {
        continue;
      }
      if (pfd->revents & POLLIN) {
        /* read some data */
        Buffer *buffer = &buffers[i];
        ssize_t bytesRead;
        bufferSetMinCapacity(
            buffer,
            buffer->length < MIN_READ_SIZE
                ? buffer->length + MIN_READ_SIZE
                : buffer->length * 2);
        errno = 0;
        bytesRead = read(
            pfd->fd,
            buffer->data + buffer->length,
            buffer->capacity - buffer->length);
        if (bytesRead < 0) {
          /* TODO: figure out how to handle errors
           * for now, we just ignore them */
        } else {
          buffer->length += (size_t)bytesRead;
        }
      }
      if (pfd->revents & POLLERR ||
          pfd->revents & POLLNVAL ||
          pfd->revents & POLLHUP) {
        /* TODO: signal errors in some way */
        /* Ignore any errors for now */
        if (pfds[i].fd >= 0) {
          close(pfds[i].fd);
          pfds[i].fd = -1;
          finished++;
        }
      }
    }
  }

  return STATUS_OK;
#else
  runtimeError("Operation not supported in this platform (readFromTwoFDs)");
  return STATUS_ERROR;
#endif
}

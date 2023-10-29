#include "mtots_util_fd.h"

#include <string.h>

#include "mtots_util_error.h"

#if MTOTS_IS_POSIX
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#endif

#define MAX_NJOBS 8
#define MAX_NFDS 8
#define MIN_READ_SIZE 4096

Status MTOTSRunFDJobs(MTOTSFDJob *jobs, size_t njobs) {
#if MTOTS_IS_POSIX
  struct pollfd pfds[MAX_NJOBS];
  size_t i, finished = 0;
  if (njobs > MAX_NJOBS) {
    runtimeError("Too many file descriptors njobs = %lu, MAX_NJOBS = %d",
                 (unsigned long)njobs, MAX_NJOBS);
    return STATUS_ERROR;
  }

  for (i = 0; i < njobs; i++) {
    int fd = pfds[i].fd = jobs[i].fd;
    pfds[i].revents = 0;
    if (fd < 0) {
      pfds[i].events = 0;
      finished++;
      continue;
    }
    switch (jobs[i].type) {
      case MTOTSFD_READ:
        pfds[i].events = POLLIN;
        break;
      case MTOTSFD_WRITE: {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
          runtimeError("fntcl(%d, F_GETFL, 0): %s", fd, strerror(errno));
          return STATUS_ERROR;
        }
        if (!(flags & O_NONBLOCK)) {
          runtimeError(
              "MTOTSRunFDJobs(): write jobs require the file descriptor "
              "to be non-blocking, but got a blocking one");
          return STATUS_ERROR;
        }
        pfds[i].events = POLLOUT;
        break;
      }
      default:
        runtimeError("Invalid FDJob type %d", jobs[i].type);
        return STATUS_ERROR;
    }
  }

  while (finished < njobs) {
    int pollResult;
    errno = 0;
    pollResult = poll(pfds, njobs, -1);
    if (pollResult < 0) {
      /* error */
      if (errno == EINTR) {
        /* signal/interrupt - we should retry */
        continue;
      }
      runtimeError("poll(): %s", strerror(errno));
      return STATUS_ERROR;
    }

    /* some file descriptors are ready */
    for (i = 0; i < njobs; i++) {
      struct pollfd *pfd = &pfds[i];
      if (pfd->revents == 0) {
        continue;
      }
      if (pfd->revents & POLLIN) {
        /* read some data */
        Buffer *buffer = jobs[i].as.read;
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
      if (pfd->revents & POLLOUT) {
        /* write some data */
        ByteSlice *slice = jobs[i].as.write;
        if (slice->start < slice->end) {
          ssize_t bytesWritten;
          errno = 0;
          bytesWritten = write(pfd->fd, slice->start, slice->end - slice->start);
          if (bytesWritten <= 0) {
            /* TODO: figure out how to handle errors
             * for now, we just ignore them */
          } else {
            slice->start += bytesWritten;
          }
        }
        if (slice->start >= slice->end) {
          close(pfd->fd);
          pfd->fd = -1;
          finished++;
          continue;
        }
      }
      if (pfd->revents & POLLERR ||
          pfd->revents & POLLNVAL ||
          pfd->revents & POLLHUP) {
        /* TODO: signal errors in some way */
        /* Ignore any errors for now */
        if (pfd->fd >= 0) {
          close(pfd->fd);
          pfd->fd = -1;
          finished++;
        }
      }
    }
  }

  return STATUS_OK;
#endif
}

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

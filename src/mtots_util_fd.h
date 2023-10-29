#ifndef mtots_util_fd_h
#define mtots_util_fd_h

/* File Descriptor Operations */

#include "mtots_util_buffer.h"

/* File Descriptor Job Type (read or write) */
typedef enum MTOTSFDType {
  MTOTSFD_READ,
  MTOTSFD_WRITE
} MTOTSFDType;

/* File Descriptor Job */
typedef struct MTOTSFDJob {
  MTOTSFDType type;
  int fd;
  union {
    Buffer *read;
    ByteSlice *write;
  } as;
} MTOTSFDJob;

/** Read and write to some file descriptors at the same time on a single thread */
Status MTOTSRunFDJobs(MTOTSFDJob *jobs, size_t njobs);

/* Read the all the data from each file descriptor into its corresponding
 * Buffer. If this function finishes successfully, all provided file
 * descriptors will be closed */
Status readFromMultipleFDs(int *fds, Buffer *buffers, size_t nfds);

#endif /*mtots_util_fd_h*/

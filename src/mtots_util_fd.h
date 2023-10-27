#ifndef mtots_util_fd_h
#define mtots_util_fd_h

/* File Descriptor Operations */

#include "mtots_util_buffer.h"

/* Read the all the data from each file descriptor into its corresponding
 * Buffer. If this function finishes successfully, all provided file
 * descriptors will be closed */
Status readFromMultipleFDs(int *fds, Buffer *buffers, size_t nfds);

#endif /*mtots_util_fd_h*/

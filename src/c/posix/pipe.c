#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

PROCESS_LIB_ERROR pipe_init(int *read, int *write)
{
  assert(read);
  assert(write);

  int pipefd[2];

  errno = 0;
  // See chapter in Design section in the readme about preventing file
  // descriptor leaks for a detailed explanation
#if defined(HAS_PIPE2)
  int result = pipe2(pipefd, O_CLOEXEC);
#else
  int result = pipe(pipefd);
  fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
#endif

  if (result == -1) {
    switch (errno) {
    case ENFILE: return PROCESS_LIB_PIPE_LIMIT_REACHED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  // Assign file descriptors if pipe() call was succesful
  *read = pipefd[0];
  *write = pipefd[1];

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer,
                             unsigned int to_write, unsigned int *actual)
{
  assert(buffer);

  errno = 0;
  ssize_t bytes_written = write(pipe, buffer, to_write);

  if (bytes_written == -1) {
    switch (errno) {
    case EPIPE: return PROCESS_LIB_STREAM_CLOSED;
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    case EIO: return PROCESS_LIB_IO_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  if (actual) { *actual = (unsigned int) bytes_written; }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, unsigned int to_read,
                            unsigned int *actual)
{
  assert(buffer);

  errno = 0;
  ssize_t bytes_read = read(pipe, buffer, to_read);

  // read is different from write in that it returns 0 to indicate the other end
  // of the pipe was closed instead of setting errno to EPIPE
  if (bytes_read == 0) { return PROCESS_LIB_STREAM_CLOSED; }
  if (bytes_read == -1) {
    switch (errno) {
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    case EIO: return PROCESS_LIB_IO_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  if (actual) { *actual = (unsigned int) bytes_read; }

  return PROCESS_LIB_SUCCESS;
}

void pipe_close(int *pipe_address)
{
  assert(pipe_address);

  int pipe = *pipe_address;

  // Do nothing and return success on null pipe (0) so callers don't have to
  // check each time if a pipe has been closed already
  if (!pipe) { return; }

  errno = 0;
  close(pipe);
  // Close should not be repeated on error so always set pipe to 0 after close
  *pipe_address = 0;
}

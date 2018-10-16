#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

REPROC_ERROR pipe_init(int *read, int *write)
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
    case ENFILE: return REPROC_PIPE_LIMIT_REACHED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  // Assign file descriptors if pipe() call was succesful
  *read = pipefd[0];
  *write = pipefd[1];

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_write(int pipe, const void *buffer, unsigned int to_write,
                        unsigned int *bytes_written)
{
  assert(buffer);
  assert(bytes_written);

  *bytes_written = 0;

  errno = 0;
  ssize_t error = write(pipe, buffer, to_write);

  if (error == -1) {
    switch (errno) {
    case EPIPE: return REPROC_STREAM_CLOSED;
    case EINTR: return REPROC_INTERRUPTED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  // If error is not -1 it is actually the amount of bytes written
  // Safe cast since we can't really have written more bytes than to_write which
  // is an unsigned int
  *bytes_written = (unsigned int) error;

  if (*bytes_written != to_write) { return REPROC_PARTIAL_WRITE; }

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_read(int pipe, void *buffer, unsigned int size,
                       unsigned int *bytes_read)
{
  assert(buffer);
  assert(bytes_read);

  *bytes_read = 0;

  errno = 0;
  ssize_t error = read(pipe, buffer, size);

  // read is different from write in that it returns 0 to indicate the other end
  // of the pipe was closed instead of setting errno to EPIPE
  if (error == 0) { return REPROC_STREAM_CLOSED; }
  if (error == -1) {
    switch (errno) {
    case EINTR: return REPROC_INTERRUPTED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  // If error is not -1 or 0 it is actually the amount of bytes read
  // Safe cast since size is an unsigned int and read will not read more bytes
  // than the buffer size
  *bytes_read = (unsigned int) error;

  return REPROC_SUCCESS;
}

void pipe_close(int *pipe)
{
  assert(pipe);

  // Do nothing and return success on null pipe (0) so callers don't have to
  // check each time if a pipe has been closed already.
  if (*pipe == 0) { return; }

  // Avoid close errors overriding other system errors.
  int last_system_error = errno;
  close(*pipe);
  errno = last_system_error;

  // Close should not be repeated on error so always set pipe to 0 after close.
  *pipe = 0;
}

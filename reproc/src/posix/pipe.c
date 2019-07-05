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

  // See avoiding resource leaks section in readme for a detailed explanation.
#if defined(PIPE2_FOUND)
  int result = pipe2(pipefd, O_CLOEXEC);
#else
  int result = pipe(pipefd);
  fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
#endif

  if (result == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  // Assign file descriptors if `pipe` call was succesfull.
  *read = pipefd[0];
  *write = pipefd[1];

  return REPROC_SUCCESS;
}

REPROC_ERROR
pipe_read(int pipe,
          uint8_t *buffer,
          unsigned int size,
          unsigned int *bytes_read)
{
  assert(buffer);
  assert(bytes_read);

  *bytes_read = 0;

  ssize_t error = read(pipe, buffer, size);
  // `read` returns 0 to indicate the other end of the pipe was closed.
  if (error == 0) {
    return REPROC_ERROR_STREAM_CLOSED;
  } else if (error == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  // If `error` is not -1 or 0 it represents the amount of bytes read.
  // The cast is safe since `size` is an unsigned int and `read` will not read
  // more than `size` bytes.
  *bytes_read = (unsigned int) error;

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_write(int pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written)
{
  assert(buffer);
  assert(bytes_written);

  *bytes_written = 0;

  ssize_t error = write(pipe, buffer, size);
  if (error == -1) {
    switch (errno) {
      // `write` sets `errno` to `EPIPE` to indicate the other end of the pipe
      // was closed.
      case EPIPE:
        return REPROC_ERROR_STREAM_CLOSED;
      default:
        return REPROC_ERROR_SYSTEM;
    }
  }

  // If `error` is not -1 it represents the amount of bytes written.
  // The cast is safe since it's impossible to write more bytes than `size`
  // which is an unsigned int.
  *bytes_written = (unsigned int) error;

  if (*bytes_written != size) {
    return REPROC_ERROR_PARTIAL_WRITE;
  }

  return REPROC_SUCCESS;
}

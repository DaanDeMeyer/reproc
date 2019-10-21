#include <posix/pipe.h>

#include <macro.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

REPROC_ERROR pipe_init(int *read, int *write)
{
  assert(read);
  assert(write);

  int pipefd[2];

  // On POSIX systems, by default file descriptors are inherited by child
  // processes when calling `exec`. To prevent unintended leaking of file
  // descriptors to child processes, POSIX provides a function `fcntl` which can
  // be used to set the `FD_CLOEXEC` flag which closes a file descriptor when
  // `exec` (or one of its variants) is called. However, using `fcntl`
  // introduces a race condition since any process spawned in another thread
  // after a file descriptor is created (for example using `pipe`) but before
  // `fcntl` is called to set `FD_CLOEXEC` on the file descriptor will still
  // inherit that file descriptor.

  // To get around this race condition we use the `pipe2` function when it
  // is available which takes the `O_CLOEXEC` flag as an argument. This ensures
  // the file descriptors of the created pipe are closed when `exec` is called.
  // If `pipe2` is not available we fall back to calling `fcntl` to set
  // `FD_CLOEXEC` immediately after creating a pipe.

#if defined(REPROC_PIPE2_FOUND)
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

REPROC_ERROR pipe_wait(int *ready, int out, int err)
{
  assert(ready);

  // See 10.0.0 changelog for why we use `poll`.

  struct pollfd fds[2] = { { .fd = out, .events = POLLIN },
                           { .fd = err, .events = POLLIN } };
  // -1 tells `poll` we want to wait indefinitely for events.
  if (poll(&fds[0], 2, -1) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  for (size_t i = 0; i < ARRAY_SIZE(fds); i++) {
    struct pollfd pollfd = fds[i];

    if (pollfd.revents & POLLIN || pollfd.revents & POLLERR) {
      *ready = pollfd.fd;
      return REPROC_SUCCESS;
    }
  }

  // Both streams should have been closed by the child process if we get here.
  assert(fds[0].revents & POLLHUP && fds[1].revents & POLLHUP);

  return REPROC_ERROR_STREAM_CLOSED;
}

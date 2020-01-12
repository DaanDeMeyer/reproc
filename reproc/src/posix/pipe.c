// Make sure we get `pipe2` on Linux.
#define _GNU_SOURCE

#include "pipe.h"

#include "error.h"
#include "macro.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <unistd.h>

int pipe_init(int *read,
              struct pipe_options read_options,
              int *write,
              struct pipe_options write_options)
{
  assert(read);
  assert(write);

  int pipefd[2] = { HANDLE_INVALID, HANDLE_INVALID };
  int r = -1;

#if defined(__APPLE__)
  r = pipe(pipefd);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto finish;
  }
#else
  // `pipe2` with `O_CLOEXEC` avoids the race condition between `pipe` and
  // `fcntl`.
  r = pipe2(pipefd, O_CLOEXEC);
  if (r < 0) {
    goto finish;
  }
#endif

  r = fcntl(pipefd[0], F_SETFL, read_options.nonblocking ? O_NONBLOCK : 0);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipefd[1], F_SETFL, write_options.nonblocking ? O_NONBLOCK : 0);
  if (r < 0) {
    goto finish;
  }

  *read = pipefd[0];
  *write = pipefd[1];

finish:
  if (r < 0) {
    handle_destroy(pipefd[0]);
    handle_destroy(pipefd[1]);
  }

  return error_unify(r);
}

int pipe_read(int pipe, uint8_t *buffer, size_t size)
{
  assert(pipe != HANDLE_INVALID);
  assert(buffer);

  int r = (int) read(pipe, buffer, size);

  // `read` returns 0 to indicate the other end of the pipe was closed.
  r = r == 0 ? -EPIPE : r;

  return error_unify_or_else(r, r);
}

int pipe_write(int pipe, const uint8_t *buffer, size_t size, int timeout)
{
  assert(pipe != HANDLE_INVALID);
  assert(buffer);

  struct pollfd pollfd = { .fd = pipe, .events = POLLOUT };
  int r = -1;

  r = poll(&pollfd, 1, timeout);
  if (r <= 0) {
    r = r == 0 ? -ETIMEDOUT : r;
    return error_unify(r);
  }

  r = (int) write(pipe, buffer, size);

  return error_unify_or_else(r, r);
}

int pipe_wait(int out, int err, int *ready, int timeout)
{
  assert(ready);

  struct pollfd pollfds[2] = { { 0 }, { 0 } };
  nfds_t num_pollfds = 0;

  if (out != HANDLE_INVALID) {
    pollfds[num_pollfds++] = (struct pollfd){ .fd = out, .events = POLLIN };
  }

  if (err != HANDLE_INVALID) {
    pollfds[num_pollfds++] = (struct pollfd){ .fd = err, .events = POLLIN };
  }

  if (num_pollfds == 0) {
    return -EPIPE;
  }

  int r = poll(pollfds, num_pollfds, timeout);
  if (r <= 0) {
    r = r == 0 ? -ETIMEDOUT : r;
    return error_unify(r);
  }

  for (size_t i = 0; i < num_pollfds; i++) {
    struct pollfd pollfd = pollfds[i];

    if (pollfd.revents > 0) {
      *ready = pollfd.fd;
      break;
    }
  }

  return 0;
}

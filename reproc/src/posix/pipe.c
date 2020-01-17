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
#include <sys/socket.h>
#include <unistd.h>

int pipe_init(int *read,
              struct pipe_options read_options,
              int *write,
              struct pipe_options write_options)
{
  assert(read);
  assert(write);

  int pipe[2] = { HANDLE_INVALID, HANDLE_INVALID };
  int r = -1;

  // We use `socketpair` so we have a POSIX compatible way of setting the pipe
  // size using `setsockopt` with `SO_RECVBUF`.

#if defined(__linux__)
  // `SOCK_CLOEXEC` avoids the race condition between `socketpair` and `fcntl`.
  r = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pipe);
  if (r < 0) {
    goto finish;
  }
#else
  r = socketpair(AF_UNIX, SOCK_STREAM, 0, pipe);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipe[0], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipe[1], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto finish;
  }
#endif

  // `socketpair` gives us a bidirectional socket pair but we only need
  // unidirectional traffic so shut down writes on the read end and vice-versa
  // on the write end.

  // `shutdown` behaves differently on macOS which causes the tests to fail. As
  // this is mostly ceremonial, we disable `shutdown` on macOS until someone
  // with a mac is impacted by this and is willing to further investigate the
  // issue.
#if !defined(__APPLE__)
  r = shutdown(pipe[0], SHUT_WR);
  if (r < 0) {
    goto finish;
  }

  r = shutdown(pipe[1], SHUT_RD);
  if (r < 0) {
    goto finish;
  }
#endif

  r = fcntl(pipe[0], F_SETFL, read_options.nonblocking ? O_NONBLOCK : 0);
  if (r < 0) {
    goto finish;
  }

  r = fcntl(pipe[1], F_SETFL, write_options.nonblocking ? O_NONBLOCK : 0);
  if (r < 0) {
    goto finish;
  }

  *read = pipe[0];
  *write = pipe[1];

  pipe[0] = HANDLE_INVALID;
  pipe[1] = HANDLE_INVALID;

finish:
  handle_destroy(pipe[0]);
  handle_destroy(pipe[1]);

  return error_unify(r);
}

int pipe_read(int pipe, uint8_t *buffer, size_t size)
{
  assert(pipe != HANDLE_INVALID);
  assert(buffer);

  int r = (int) read(pipe, buffer, size);
  if (r == 0) {
    // `read` returns 0 to indicate the other end of the pipe was closed.
    r = -EPIPE;
  }

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
    return error_unify_or_else(r, -ETIMEDOUT);
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
    return error_unify_or_else(r, -ETIMEDOUT);
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

// Make sure we get `pipe2` on Linux.
#define _GNU_SOURCE

#include "pipe.h"

#include "error.h"
#include "handle.h"
#include "macro.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

const int PIPE_INVALID = -1;

int pipe_init(int *read, int *write)
{
  assert(read);
  assert(write);

  int pipe[2] = { PIPE_INVALID, PIPE_INVALID };
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

  *read = pipe[0];
  *write = pipe[1];

  pipe[0] = PIPE_INVALID;
  pipe[1] = PIPE_INVALID;

finish:
  pipe_destroy(pipe[0]);
  pipe_destroy(pipe[1]);

  return error_unify(r);
}

int pipe_nonblocking(int pipe, bool enable)
{
  int r = fcntl(pipe, F_SETFL, enable ? O_NONBLOCK : 0);
  return error_unify(r);
}

int pipe_read(int pipe, uint8_t *buffer, size_t size)
{
  assert(pipe != PIPE_INVALID);
  assert(buffer);

  int r = (int) read(pipe, buffer, size);

  if (r == 0) {
    // `read` returns 0 to indicate the other end of the pipe was closed.
    r = -EPIPE;
  }

  return error_unify_or_else(r, r);
}

int pipe_write(int pipe, const uint8_t *buffer, size_t size)
{
  assert(pipe != PIPE_INVALID);
  assert(buffer);

  int r = (int) write(pipe, buffer, size);

  return error_unify_or_else(r, r);
}

int pipe_wait(pipe_set *sets, size_t num_sets, int timeout)
{
  assert(num_sets * PIPES_PER_SET <= INT_MAX);

  struct pollfd *pollfds = NULL;
  size_t num_pipes = num_sets * PIPES_PER_SET;
  int r = -ENOMEM;

  pollfds = calloc(sizeof(struct pollfd), num_pipes);
  if (pollfds == NULL) {
    goto finish;
  }

  for (size_t i = 0; i < num_sets; i++) {
    size_t j = i * PIPES_PER_SET;
    pollfds[j + 0] = (struct pollfd){ .fd = sets[i].in, .events = POLLOUT };
    pollfds[j + 1] = (struct pollfd){ .fd = sets[i].out, .events = POLLIN };
    pollfds[j + 2] = (struct pollfd){ .fd = sets[i].err, .events = POLLIN };
  }

  r = poll(pollfds, (nfds_t) num_pipes, timeout);
  if (r < 0) {
    goto finish;
  }

  for (size_t i = 0; i < num_sets; i++) {
    sets[i].events = 0;
  }

  for (size_t i = 0; i < num_pipes; i++) {
    struct pollfd pollfd = pollfds[i];

    if (pollfd.revents > 0) {
      int event = 1 << (i % PIPES_PER_SET);
      sets[i / PIPES_PER_SET].events |= event;
    }
  }

  if (r == 0) {
    r = -ETIMEDOUT;
  }

finish:
  free(pollfds);

  return r;
}

int pipe_destroy(int pipe)
{
  return handle_destroy(pipe);
}

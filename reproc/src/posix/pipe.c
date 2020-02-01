// Make sure we get `pipe2` on Linux.
#define _GNU_SOURCE

#include "pipe.h"

#include "error.h"
#include "handle.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

const int PIPE_INVALID = -1;

static int pipe_cloexec(int pipe)
{
  int r = -1;

  r = fcntl(pipe, F_GETFD, 0);
  if (r < 0) {
    return error_unify(r);
  }

  r |= FD_CLOEXEC;

  r = fcntl(pipe, F_SETFD, r);
  if (r < 0) {
    return error_unify(r);
  }

  return 0;
}

int pipe_init(int *read, int *write)
{
  assert(read);
  assert(write);

  int pair[] = { PIPE_INVALID, PIPE_INVALID };
  int r = -1;

#if defined(__APPLE__)
  r = pipe(pair);
  if (r < 0) {
    goto finish;
  }
#else
  // `pipe2` with `O_CLOEXEC` avoids the race condition between `pipe` and
  // `fcntl`.
  r = pipe2(pair, O_CLOEXEC);
  if (r < 0) {
    goto finish;
  }
#endif

  r = pipe_cloexec(pair[0]);
  if (r < 0) {
    goto finish;
  }

  r = pipe_cloexec(pair[1]);
  if (r < 0) {
    goto finish;
  }

  *read = pair[0];
  *write = pair[1];

  pair[0] = PIPE_INVALID;
  pair[1] = PIPE_INVALID;

finish:
  pipe_destroy(pair[0]);
  pipe_destroy(pair[1]);

  return error_unify(r);
}

int pipe_nonblocking(int pipe, bool enable)
{
  int r = -1;

  r = fcntl(pipe, F_GETFL, 0);
  if (r < 0) {
    return error_unify(r);
  }

  r = enable ? r | O_NONBLOCK : r & ~O_NONBLOCK;

  r = fcntl(pipe, F_SETFL, r);

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
    // macos 10.15 indicates `POLLIN` instead of `POLLHUP` when the peer fd is
    // closed.
    pollfds[j + 3] = (struct pollfd){ .fd = sets[i].exit, .events = POLLIN };
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

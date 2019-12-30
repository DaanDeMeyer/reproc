// Make sure we get `pipe2` on Linux.
#define _GNU_SOURCE

#include <pipe.h>

#include "error.h"
#include "macro.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
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
    goto cleanup;
  }

  r = fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto cleanup;
  }

  r = fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto cleanup;
  }
#else
  // `pipe2` with `O_CLOEXEC` avoids the race condition between `pipe` and
  // `fcntl`.
  r = pipe2(pipefd, O_CLOEXEC);
  if (r < 0) {
    goto cleanup;
  }
#endif

  r = fcntl(pipefd[0], F_SETFL, read_options.nonblocking ? O_NONBLOCK : 0);
  if (r < 0) {
    goto cleanup;
  }

  r = fcntl(pipefd[1], F_SETFL, write_options.nonblocking ? O_NONBLOCK : 0);
  if (r < 0) {
    goto cleanup;
  }

  *read = pipefd[0];
  *write = pipefd[1];

cleanup:
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

  ssize_t r = read(pipe, buffer, size);
  assert(r <= INT_MAX);

  // `read` returns 0 to indicate the other end of the pipe was closed.
  r = r == 0 ? -EPIPE : r;

  return error_unify_or_else((int) r, (int) r);
}

int pipe_write(int pipe, const uint8_t *buffer, size_t size)
{
  assert(pipe != HANDLE_INVALID);
  assert(buffer);

  ssize_t r = write(pipe, buffer, size);
  assert(r <= INT_MAX);

  return error_unify_or_else((int) r, (int) r);
}

static const int POLL_INFINITE = -1;

int pipe_wait(int out, int err, int *ready)
{
  assert(out != HANDLE_INVALID);
  assert(err != HANDLE_INVALID);
  assert(ready);

  struct pollfd pollfds[2] = { { .fd = out, .events = POLLIN },
                               { .fd = err, .events = POLLIN } };

  int r = poll(pollfds, ARRAY_SIZE(pollfds), POLL_INFINITE);
  if (r < 0) {
    return error_unify(r);
  }

  for (size_t i = 0; i < ARRAY_SIZE(pollfds); i++) {
    struct pollfd pollfd = pollfds[i];

    if (pollfd.revents > 0) {
      *ready = pollfd.fd;
      break;
    }
  }

  return 0;
}

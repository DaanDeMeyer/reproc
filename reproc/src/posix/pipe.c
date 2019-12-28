#include <pipe.h>

#include "error.h"

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

  int pipefd[2] = { -1, -1 };
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

  return error_unify(r, 0);
}

int pipe_read(int pipe, uint8_t *buffer, size_t size)
{
  assert(buffer);

  ssize_t r = read(pipe, buffer, size);
  assert(r <= INT_MAX);

  // `read` returns 0 to indicate the other end of the pipe was closed.
  if (r == 0) {
    r = -1;
    errno = EPIPE;
  }

  return error_unify((int) r, (int) r);
}

static const int POLL_INFINITE = -1;

int pipe_write(int pipe, const uint8_t *buffer, size_t size)
{
  assert(buffer);

  struct pollfd pollfd = { .fd = pipe, .events = POLLOUT };
  ssize_t r = -1;

  r = poll(&pollfd, 1, POLL_INFINITE);
  if (r < 0) {
    goto cleanup;
  }

  assert(pollfd.revents & POLLOUT || pollfd.revents & POLLERR);

  r = write(pipe, buffer, size);
  assert(r <= INT_MAX);

cleanup:
  return error_unify((int) r, (int) r);
}

int pipe_wait(const handle *pipes, size_t num_pipes)
{
  assert(pipes);
  assert(num_pipes <= INT_MAX);

  size_t i = 0;
  int r = -1;

  struct pollfd *pollfds = calloc(num_pipes, sizeof(struct pollfd));
  if (pollfds == NULL) {
    r = -1;
    goto cleanup;
  }

  for (i = 0; i < num_pipes; i++) {
    pollfds[i].fd = pipes[i];
    pollfds[i].events = POLLIN;
  }

  r = poll(pollfds, (unsigned int) num_pipes, POLL_INFINITE);
  if (r < 0) {
    goto cleanup;
  }

  for (i = 0; i < num_pipes; i++) {
    struct pollfd pollfd = pollfds[i];

    if (pollfd.revents & POLLIN || pollfd.revents & POLLERR) {
      r = (int) i;
      break;
    }
  }

  if (i == num_pipes) {
    r = -1;
    errno = EPIPE;
  }

cleanup:
  free(pollfds);

  return error_unify(r, r);
}

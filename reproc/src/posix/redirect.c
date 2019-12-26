#include <redirect.h>

#include <error.h>
#include <pipe.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

static const char *DEVNULL = "/dev/null";

static const struct pipe_options CHILD_OPTIONS = { .nonblocking = false };
static const struct pipe_options PARENT_OPTIONS = { .nonblocking = true };

int redirect_pipe(int *parent, int *child, REDIRECT_STREAM stream)
{
  assert(parent);
  assert(child);

  return stream == REDIRECT_STREAM_IN
             ? pipe_init(child, CHILD_OPTIONS, parent, PARENT_OPTIONS)
             : pipe_init(parent, PARENT_OPTIONS, child, CHILD_OPTIONS);
}

int redirect_inherit(int *parent, int *child, REDIRECT_STREAM stream)
{
  assert(parent);
  assert(child);

  FILE *file = stream == REDIRECT_STREAM_IN
                   ? stdin
                   : stream == REDIRECT_STREAM_OUT ? stdout : stderr;
  int r = -1;

  r = fileno(file);
  if (r < 0) {
    errno = errno == EBADF ? -EPIPE : -errno;
    goto cleanup;
  }

  int fd = r;

  r = fcntl(fd, F_DUPFD_CLOEXEC, 0);
  if (r < 0) {
    goto cleanup;
  }

  *parent = HANDLE_INVALID;
  *child = fd;

cleanup:
  return error_unify(r, 0);
}

int redirect_discard(int *parent, int *child, REDIRECT_STREAM stream)
{
  assert(parent);
  assert(child);

  int mode = stream == REDIRECT_STREAM_IN ? O_RDONLY : O_WRONLY;

  int r = open(DEVNULL, mode | O_CLOEXEC);
  if (r < 0) {
    goto cleanup;
  }

  int fd = r;

  *parent = HANDLE_INVALID;
  *child = fd;

cleanup:
  return error_unify(r, 0);
}

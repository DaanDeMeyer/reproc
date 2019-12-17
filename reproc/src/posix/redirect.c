#include <redirect.h>

#include <pipe.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

static const char *DEVNULL = "/dev/null";

static const struct pipe_options CHILD_OPTIONS = { .nonblocking = false };
static const struct pipe_options PARENT_OPTIONS = { .nonblocking = true };

REPROC_ERROR redirect_pipe(int *parent, int *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  return stream == REPROC_STREAM_IN
             ? pipe_init(child, CHILD_OPTIONS, parent, PARENT_OPTIONS)
             : pipe_init(parent, PARENT_OPTIONS, child, CHILD_OPTIONS);
}

REPROC_ERROR redirect_inherit(int *parent, int *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  *parent = HANDLE_INVALID;
  FILE *file = stream == REPROC_STREAM_IN
                   ? stdin
                   : stream == REPROC_STREAM_OUT ? stdout : stderr;

  int fd = fileno(file);
  if (fd == -1) {
    return REPROC_ERROR_STREAM_CLOSED;
  }

  *child = fcntl(fd, F_DUPFD_CLOEXEC, 0);

  return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

REPROC_ERROR redirect_discard(int *parent, int *child, REPROC_STREAM stream)
{
  assert(parent);
  assert(child);

  *parent = HANDLE_INVALID;
  int mode = stream == REPROC_STREAM_IN ? O_RDONLY : O_WRONLY;

  *child = open(DEVNULL, mode | O_CLOEXEC);

  return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

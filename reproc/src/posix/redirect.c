#include <redirect.h>

#include <pipe.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

static const char *DEVNULL = "/dev/null";

static const struct pipe_options CHILD_OPTIONS = { .nonblocking = false };
static const struct pipe_options PARENT_OPTIONS = { .nonblocking = true };

REPROC_ERROR
redirect(int *parent, int *child, REPROC_STREAM stream, REPROC_REDIRECT type)
{
  assert(parent);
  assert(child);

  *parent = 0;

  int fd = -1;

  if (type == REPROC_REDIRECT_INHERIT) {
    // `REPROC_STREAM` => `FILE *`
    FILE *file = stream == REPROC_STREAM_IN
                     ? stdin
                     : stream == REPROC_STREAM_OUT ? stdout : stderr;
    fd = fileno(file);

    // If we're inheriting a parent stream and the stream is not a valid stream
    // (`fd == -1`), we interpret this as if the child process is inheriting a
    // closed stream and discard all output from the child process stream.
    if (fd == -1) {
      type = REPROC_REDIRECT_DISCARD;
    }
  }

  switch (stream) {

    case REPROC_STREAM_IN:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(child, CHILD_OPTIONS, parent, PARENT_OPTIONS);

        case REPROC_REDIRECT_INHERIT:
          *child = fcntl(fd, F_DUPFD_CLOEXEC, 0);
          return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;

        case REPROC_REDIRECT_DISCARD:
          *child = open(DEVNULL, O_RDONLY | O_CLOEXEC);
          return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
      }

      assert(false);
      break;

    case REPROC_STREAM_OUT:
    case REPROC_STREAM_ERR:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(parent, PARENT_OPTIONS, child, CHILD_OPTIONS);

        case REPROC_REDIRECT_INHERIT:
          *child = fcntl(fd, F_DUPFD_CLOEXEC, 0);
          return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;

        case REPROC_REDIRECT_DISCARD:
          *child = open(DEVNULL, O_WRONLY | O_CLOEXEC);
          return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
      }
  }

  assert(false);
  return REPROC_ERROR_SYSTEM;
}

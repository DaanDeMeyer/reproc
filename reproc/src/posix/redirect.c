#include <posix/redirect.h>

#include <posix/pipe.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

static const char *DEVNULL = "/dev/null";

REPROC_ERROR
redirect(int *parent, int *child, REPROC_STREAM stream, REPROC_REDIRECT type)
{
  assert(parent);
  assert(child);

  const struct pipe_options blocking = { .nonblocking = false };
  const struct pipe_options nonblocking = { .nonblocking = true };

  *parent = 0;

  switch (stream) {

    case REPROC_STREAM_IN:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(child, blocking, parent, nonblocking);

        case REPROC_REDIRECT_INHERIT:
          *child = fileno(stdin);
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
          return pipe_init(parent, nonblocking, child, blocking);

        case REPROC_REDIRECT_INHERIT:
          *child = fileno(stream == REPROC_STREAM_OUT ? stdout : stderr);
          return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;

        case REPROC_REDIRECT_DISCARD:
          *child = open(DEVNULL, O_WRONLY | O_CLOEXEC);
          return *child == -1 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
      }
  }

  assert(false);
  return REPROC_ERROR_SYSTEM;
}

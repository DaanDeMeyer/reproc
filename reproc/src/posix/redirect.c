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

  switch (stream) {

    case REPROC_STREAM_IN:

      switch (type) {
        case REPROC_REDIRECT_PIPE:
          return pipe_init(child, CHILD_OPTIONS, parent, PARENT_OPTIONS);

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
          return pipe_init(parent, PARENT_OPTIONS, child, CHILD_OPTIONS);

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

#define _POSIX_C_SOURCE 200809L

#include "redirect.h"

#include "error.h"
#include "pipe.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

static FILE *stream_to_file(REPROC_STREAM stream)
{
  switch (stream) {
    case REPROC_STREAM_IN:
      return stdin;
    case REPROC_STREAM_OUT:
      return stdout;
    case REPROC_STREAM_ERR:
      return stderr;
  }

  return NULL;
}

int redirect_parent(int *child, REPROC_STREAM stream)
{
  ASSERT(child);

  int r = -EINVAL;

  FILE *file = stream_to_file(stream);
  if (file == NULL) {
    return r;
  }

  r = fileno(file);
  if (r < 0) {
    if (errno == EBADF) {
      r = -EPIPE;
    }

    return error_unify(r);
  }

  *child = r; // `r` contains the duplicated file descriptor.

  return 0;
}

int redirect_discard(int *child, REPROC_STREAM stream)
{
  return redirect_path(child, stream, "/dev/null");
}

int redirect_file(int *child, FILE *file)
{
  ASSERT(child);

  int r = fileno(file);
  if (r < 0) {
    return error_unify(r);
  }

  *child = r;

  return 0;
}

int redirect_path(int *child, REPROC_STREAM stream, const char *path)
{
  ASSERT(child);
  ASSERT(path);

  int mode = stream == REPROC_STREAM_IN ? O_RDONLY : O_WRONLY;

  int r = open(path, mode | O_CREAT | O_CLOEXEC, 0640);
  if (r < 0) {
    return error_unify(r);
  }

  *child = r;

  return 0;
}

#include "fd.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

void fd_close(int *fd)
{
  assert(fd);

  // Do nothing and return if `fd` is 0 (null) so callers don't have to check if
  // a pipe has been closed already.
  if (*fd == 0) { return; }

  // Avoid `close` errors overriding other system errors.
  int last_system_error = errno;
  close(*fd);
  errno = last_system_error;

  // `close` should not be repeated on error so always set `fd` to 0.
  *fd = 0;
}

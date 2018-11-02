#include "fd.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

void fd_close(int *fd)
{
  assert(fd);

  // Do nothing and return success on null pipe (0) so callers don't have to
  // check each time if a pipe has been closed already.
  if (*fd == 0) { return; }

  // Avoid close errors overriding other system errors.
  int last_system_error = errno;
  close(*fd);
  errno = last_system_error;

  // Close should not be repeated on error so always set pipe to 0 after close.
  *fd = 0;
}

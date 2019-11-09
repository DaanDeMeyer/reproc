#include <handle.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>

void handle_close(int *handle)
{
  assert(handle);

  // Do nothing and return if `handle` is 0 (null) so callers don't have to
  // check if a handle has been closed already.
  if (*handle == 0) {
    return;
  }

  // Avoid `close` errors overriding other system errors.
  int last_system_error = errno;
  close(*handle);
  errno = last_system_error;

  // `close` should not be repeated on error so always set `handle` to 0.
  *handle = 0;
}

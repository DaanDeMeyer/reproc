#include <handle.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>

const int HANDLE_INVALID = -1;

int handle_destroy(int handle)
{
  if (handle == HANDLE_INVALID) {
    return HANDLE_INVALID;
  }

  // Avoid `close` errors overriding other system errors.
  int last_system_error = errno;
  close(handle);
  errno = last_system_error;

  return HANDLE_INVALID;
}

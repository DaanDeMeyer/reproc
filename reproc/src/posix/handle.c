#include <handle.h>

#include "error.h"

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
  PROTECT_SYSTEM_ERROR(close(handle));

  return HANDLE_INVALID;
}

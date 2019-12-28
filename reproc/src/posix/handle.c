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

  int r = 0;

  PROTECT_SYSTEM_ERROR;

  // Avoid `close` errors overriding other system errors.
  r = close(handle);
  assert_unused(r == 0);

  UNPROTECT_SYSTEM_ERROR;

  return HANDLE_INVALID;
}

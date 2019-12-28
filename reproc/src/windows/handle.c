#include "handle.h"

#include "error.h"

#include <assert.h>
#include <windows.h>

const HANDLE HANDLE_INVALID = INVALID_HANDLE_VALUE; // NOLINT

HANDLE handle_destroy(HANDLE handle)
{
  if (handle == NULL || handle == HANDLE_INVALID) {
    return HANDLE_INVALID;
  }

  BOOL r = 0;

  PROTECT_SYSTEM_ERROR;

  // Avoid `CloseHandle` errors overriding other system errors.
  r = CloseHandle(handle);
  assert_unused(r != 0);

  UNPROTECT_SYSTEM_ERROR;

  return HANDLE_INVALID;
}

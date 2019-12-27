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

  // Avoid `CloseHandle` errors overriding other system errors.
  PROTECT_SYSTEM_ERROR(CloseHandle(handle));

  return HANDLE_INVALID;
}

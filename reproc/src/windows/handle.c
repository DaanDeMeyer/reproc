#include <handle.h>

#include <assert.h>
#include <windows.h>

const HANDLE HANDLE_INVALID = INVALID_HANDLE_VALUE; // NOLINT

HANDLE handle_destroy(HANDLE handle)
{
  if (handle == NULL || handle == HANDLE_INVALID) {
    return HANDLE_INVALID;
  }

  // Avoid `CloseHandle` errors overriding other system errors.
  unsigned int last_error = GetLastError();
  CloseHandle(handle);
  SetLastError(last_error);

  return HANDLE_INVALID;
}

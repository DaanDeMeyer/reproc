#include <handle.h>

#include <assert.h>
#include <windows.h>

void handle_close(HANDLE *handle)
{
  assert(handle);

  // Do nothing if `handle` is `NULL` so callers don't have to check if `handle`
  // has already been closed.
  if (*handle == NULL || *handle == INVALID_HANDLE_VALUE) {
    return;
  }

  // Avoid `CloseHandle` errors overriding other system errors.
  unsigned int last_error = GetLastError();
  CloseHandle(*handle);
  SetLastError(last_error);

  // `CloseHandle` should not be repeated if an error occurs so we always set
  // `handle` to `NULL`.
  *handle = NULL;
}

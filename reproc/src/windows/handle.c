#include <windows/handle.h>

#include <assert.h>

REPROC_ERROR handle_disable_inherit(HANDLE pipe)
{
  assert(pipe);

  if (!SetHandleInformation(pipe, HANDLE_FLAG_INHERIT, 0)) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

void handle_close(HANDLE *handle)
{
  assert(handle);

  // Do nothing if `handle` is `NULL` so callers don't have to check if `handle`
  // has already been closed.
  if (!*handle) {
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

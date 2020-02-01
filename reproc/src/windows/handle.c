#include "handle.h"

#include "error.h"

#include <assert.h>
#include <io.h>
#include <windows.h>

const HANDLE HANDLE_INVALID = INVALID_HANDLE_VALUE; // NOLINT

int handle_from(FILE *file, HANDLE *handle)
{
  int r = -1;

  r = _fileno(file);
  if (r < 0) {
    return -ERROR_INVALID_HANDLE;
  }

  intptr_t result = _get_osfhandle(r);
  if (result == -1) {
    return -ERROR_INVALID_HANDLE;
  }

  *handle = (HANDLE) result;

  return error_unify(r);
}

HANDLE handle_destroy(HANDLE handle)
{
  if (handle == NULL || handle == HANDLE_INVALID) {
    return HANDLE_INVALID;
  }

  int r = 0;

  PROTECT_SYSTEM_ERROR;

  // Avoid `CloseHandle` errors overriding other system errors.
  r = CloseHandle(handle);
  assert_unused(r != 0);

  UNPROTECT_SYSTEM_ERROR;

  return HANDLE_INVALID;
}

#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "handle.h"

#include "error.h"

#include <assert.h>
#include <windows.h>

const HANDLE HANDLE_INVALID = INVALID_HANDLE_VALUE; // NOLINT

int handle_cloexec(handle_type handle, bool enable)
{
  (void) handle;
  (void) enable;
  return -ERROR_CALL_NOT_IMPLEMENTED;
}

HANDLE handle_destroy(HANDLE handle)
{
  if (handle == NULL || handle == HANDLE_INVALID) {
    return HANDLE_INVALID;
  }

  int r = 0;

  r = CloseHandle(handle);
  ASSERT_UNUSED(r != 0);

  return HANDLE_INVALID;
}

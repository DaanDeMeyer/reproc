#include "handle.h"

#include <assert.h>

void handle_close(HANDLE *handle)
{
  assert(handle);

  // Do nothing on NULL handle so callers don't have to check each time if a
  // handle has been closed already.
  if (!*handle) { return; }

  // Avoid handle close errors overriding other system errors.
  unsigned int last_error = GetLastError();
  CloseHandle(*handle);
  SetLastError(last_error);

  // CloseHandle should not be repeated on error so always set handle to NULL
  // after calling CloseHandle.
  *handle = NULL;
}

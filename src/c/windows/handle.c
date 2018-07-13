#include "handle.h"

#include <assert.h>
#include <stdlib.h>

void handle_close(HANDLE *handle_address)
{
  assert(handle_address);

  HANDLE handle = *handle_address;

  // Do nothing and return success on null handle so callers don't have to check
  // each time if a handle has been closed already
  if (!handle) { return; }

  SetLastError(0);
  CloseHandle(handle);
  // CloseHandle should not be repeated on error so always set handle to NULL
  // after CloseHandle
  *handle_address = NULL;
}



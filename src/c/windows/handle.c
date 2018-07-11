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

PROCESS_LIB_ERROR
handle_inherit_list_create(HANDLE *handles, int amount,
                           LPPROC_THREAD_ATTRIBUTE_LIST *result)
{
  SIZE_T attribute_list_size = 0;
  SetLastError(0);
  if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = malloc(attribute_list_size);
  if (!attribute_list) { return PROCESS_LIB_MEMORY_ERROR; }

  SetLastError(0);
  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0,
                                         &attribute_list_size)) {
    free(attribute_list);
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  SetLastError(0);
  if (!UpdateProcThreadAttribute(attribute_list, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                 amount * sizeof(HANDLE), NULL, NULL)) {
    free(attribute_list);
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  *result = attribute_list;

  return PROCESS_LIB_SUCCESS;
}

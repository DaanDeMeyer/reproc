#include "util.h"

#include <assert.h>
#include <malloc.h>
#include <string.h>

// Ensures pipe is inherited by child process
static SECURITY_ATTRIBUTES security_attributes = {
    .nLength = sizeof(SECURITY_ATTRIBUTES),
    .bInheritHandle = TRUE,
    .lpSecurityDescriptor = NULL};

PROCESS_LIB_ERROR pipe_init(HANDLE *read, HANDLE *write)
{
  assert(read);
  assert(write);

  SetLastError(0);
  if (!CreatePipe(read, write, &security_attributes, 0)) {
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_disable_inherit(HANDLE pipe)
{
  assert(pipe);

  SetLastError(0);
  if (!SetHandleInformation(pipe, HANDLE_FLAG_INHERIT, 0)) {
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_write(HANDLE pipe, const void *buffer, uint32_t to_write,
                             uint32_t *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  // Cast is valid since DWORD = unsigned int on Windows
  SetLastError(0);
  if (!WriteFile(pipe, buffer, to_write, (LPDWORD) actual, NULL)) {
    switch (GetLastError()) {
    case ERROR_OPERATION_ABORTED: return PROCESS_LIB_INTERRUPTED;
    case ERROR_BROKEN_PIPE: return PROCESS_LIB_STREAM_CLOSED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_read(HANDLE pipe, void *buffer, uint32_t to_read,
                            uint32_t *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  SetLastError(0);
  if (!ReadFile(pipe, buffer, to_read, (LPDWORD) actual, NULL)) {
    switch (GetLastError()) {
    case ERROR_OPERATION_ABORTED: return PROCESS_LIB_INTERRUPTED;
    case ERROR_BROKEN_PIPE: return PROCESS_LIB_STREAM_CLOSED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR string_join(const char **string_array, int array_length,
                              char **result)
{
  assert(string_array);
  assert(array_length >= 0);
  assert(result);

  for (int i = 0; i < array_length; i++) {
    assert(string_array[i]);
  }

  size_t string_length = 1; // Null terminator
  for (int i = 0; i < array_length; i++) {
    string_length += strlen(string_array[i]);
    if (i < array_length - 1) { string_length++; } // whitespace
  }

  char *string = malloc(sizeof(char) * string_length);
  if (string == NULL) { return PROCESS_LIB_MEMORY_ERROR; }

  char *current = string; // iterator
  for (int i = 0; i < array_length; i++) {
    size_t part_length = strlen(string_array[i]);
    strcpy(current, string_array[i]);
    current += part_length;

    if (i < array_length - 1) {
      *current = ' ';
      current += 1;
    }
  }

  *current = '\0';

  *result = string;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR string_to_wstring(const char *string, wchar_t **result)
{
  assert(string);
  assert(result);

  SetLastError(0);
  int wstring_length = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
  if (wstring_length == 0) {
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return PROCESS_LIB_INVALID_UNICODE;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  wchar_t *wstring = malloc(sizeof(wchar_t) * wstring_length);
  if (!wstring) { return PROCESS_LIB_MEMORY_ERROR; }

  SetLastError(0);
  int written = MultiByteToWideChar(CP_UTF8, 0, string, -1, wstring,
                                    wstring_length);
  if (written == 0) {
    switch (GetLastError()) {
    case ERROR_NO_UNICODE_TRANSLATION: return PROCESS_LIB_INVALID_UNICODE;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *result = wstring;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR handle_close(HANDLE *handle_address)
{
  assert(handle_address);

  HANDLE handle = *handle_address;

  // Do nothing and return success on null handle so callers don't have to check
  // each time if a handle has been closed already
  if (!handle) { return PROCESS_LIB_SUCCESS; }

  SetLastError(0);
  DWORD result = CloseHandle(handle);
  *handle_address = NULL; // Resources should only be closed once

  if (!result) { return PROCESS_LIB_UNKNOWN_ERROR; }

  return PROCESS_LIB_SUCCESS;
}

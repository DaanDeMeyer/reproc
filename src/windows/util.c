#include "util.h"

#include <assert.h>
#include <malloc.h>

// Ensures pipe is inherited by child process
static SECURITY_ATTRIBUTES security_attributes = {
    .nLength = sizeof(SECURITY_ATTRIBUTES),
    .bInheritHandle = TRUE,
    .lpSecurityDescriptor = NULL};

PROCESS_ERROR pipe_init(HANDLE *read, HANDLE *write, HANDLE *do_not_inherit)
{
  assert(read);
  assert(write);
  assert(do_not_inherit);

  SetLastError(0);

  if (!CreatePipe(read, write, &security_attributes, 0)) {
    return system_error_to_process_error(GetLastError());
  }

  if (!SetHandleInformation(*do_not_inherit, HANDLE_FLAG_INHERIT, 0)) {
    return system_error_to_process_error(GetLastError());
  }

  return PROCESS_SUCCESS;
}

PROCESS_ERROR pipe_write(HANDLE pipe, const void *buffer, uint32_t to_write,
                         uint32_t *actual)
{
  assert(pipe);
  assert(buffer);

  SetLastError(0);

  // Cast is valid since DWORD = unsigned int on Windows
  if (!WriteFile(pipe, buffer, to_write, (LPDWORD)actual, NULL)) {
    return system_error_to_process_error(GetLastError());
  }

  return PROCESS_SUCCESS;
}

PROCESS_ERROR pipe_read(HANDLE pipe, void *buffer, uint32_t to_read,
                        uint32_t *actual)
{
  assert(pipe);
  assert(buffer);

  SetLastError(0);

  if (!ReadFile(pipe, buffer, to_read, (LPDWORD)actual, NULL)) {
    return system_error_to_process_error(GetLastError());
  }

  return PROCESS_SUCCESS;
}

char *string_join(char **string_array, int array_length)
{
  assert(string_array);
  assert(array_length >= 0);

  for (int i = 0; i < array_length; i++) {
    assert(string_array[i]);
  }

  size_t string_length = 1; // Null terminator
  for (int i = 0; i < array_length; i++) {
    string_length += strlen(string_array[i]);
    if (i < array_length - 1) { string_length++; } // whitespace
  }

  char *string = malloc(sizeof(char) * string_length);
  if (string == NULL) { return NULL; }

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

  return string;
}

wchar_t *string_to_wstring(const char *string)
{
  assert(string);

  int wstring_length = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
  wchar_t *wstring = malloc(sizeof(wchar_t) * wstring_length);
  MultiByteToWideChar(CP_UTF8, 0, string, -1, wstring, wstring_length);

  return wstring;
}

PROCESS_ERROR system_error_to_process_error(DWORD system_error)
{
  switch (system_error) {
  case ERROR_SUCCESS:
    return PROCESS_SUCCESS;
  case ERROR_BROKEN_PIPE:
    return PROCESS_STREAM_CLOSED;
  case ERROR_INVALID_HANDLE:
    return PROCESS_CLOSE_ERROR;
  default:
    return PROCESS_UNKNOWN_ERROR;
  }
}
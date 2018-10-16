#include "pipe.h"

#include <assert.h>

// Ensures pipe is inherited by child process.
static SECURITY_ATTRIBUTES security_attributes = {
  .nLength = sizeof(SECURITY_ATTRIBUTES),
  .bInheritHandle = TRUE,
  .lpSecurityDescriptor = NULL
};

REPROC_ERROR pipe_init(HANDLE *read, HANDLE *write)
{
  assert(read);
  assert(write);

  SetLastError(0);
  if (!CreatePipe(read, write, &security_attributes, 0)) {
    return REPROC_UNKNOWN_ERROR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_disable_inherit(HANDLE pipe)
{
  assert(pipe);

  SetLastError(0);
  if (!SetHandleInformation(pipe, HANDLE_FLAG_INHERIT, 0)) {
    return REPROC_UNKNOWN_ERROR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_write(HANDLE pipe, const void *buffer, unsigned int to_write,
                        unsigned int *bytes_written)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_written);

  SetLastError(0);
  // Cast is valid since DWORD = unsigned int on Windows.
  // WriteFile always sets bytes_written to 0 so we don't do it ourselves.
  if (!WriteFile(pipe, buffer, to_write, (LPDWORD) bytes_written, NULL)) {
    switch (GetLastError()) {
    case ERROR_OPERATION_ABORTED: return REPROC_INTERRUPTED;
    case ERROR_BROKEN_PIPE: return REPROC_STREAM_CLOSED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  if (*bytes_written != to_write) { return REPROC_PARTIAL_WRITE; }

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_read(HANDLE pipe, void *buffer, unsigned int size,
                       unsigned int *bytes_read)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_read);

  SetLastError(0);
  // Cast is valid since DWORD = unsigned int on Windows.
  // ReadFile always sets bytes_read to 0 so we don't do it ourselves.
  if (!ReadFile(pipe, buffer, size, (LPDWORD) bytes_read, NULL)) {
    switch (GetLastError()) {
    case ERROR_OPERATION_ABORTED: return REPROC_INTERRUPTED;
    case ERROR_BROKEN_PIPE: return REPROC_STREAM_CLOSED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  return REPROC_SUCCESS;
}

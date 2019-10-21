#include <windows/pipe.h>

#include <macro.h>
#include <windows/handle.h>

#include <assert.h>

// Ensures both endpoints of the pipe are inherited by the child process.
static SECURITY_ATTRIBUTES security_attributes = {
  .nLength = sizeof(SECURITY_ATTRIBUTES),
  .bInheritHandle = TRUE,
  .lpSecurityDescriptor = NULL
};

// Disables a single endpoint of a pipe from being inherited by the child
// process.
static REPROC_ERROR pipe_disable_inherit(HANDLE pipe)
{
  assert(pipe);

  if (!SetHandleInformation(pipe, HANDLE_FLAG_INHERIT, 0)) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR
pipe_init(HANDLE *read, bool inherit_read, HANDLE *write, bool inherit_write)
{
  assert(read);
  assert(write);

  // On Windows the `CreatePipe` function receives a flag as part of its
  // arguments that specifies if the returned handles can be inherited by child
  // processes or not. Inheritance for endpoints of a single pipe can be
  // configured after the `CreatePipe` call using the function
  // `SetHandleInformation`. A race condition occurs after calling `CreatePipe`
  // (allowing inheritance) but before calling `SetHandleInformation` in one
  // thread and calling `CreateProcess` (configured to inherit pipes) in another
  // thread. In this scenario handles are unintentionally leaked into a child
  // process. We try to mitigate this by calling `SetHandleInformation` after
  // `CreatePipe` for the handles that should not be inherited by any process to
  // lower the chance of them accidentally being inherited (just like with
  // `fnctl` on POSIX if `pipe2` is not available). This only works for half of
  // the endpoints created (the ones intended to be used by the parent process)
  // since the endpoints intended to be used by the child actually need to be
  // inherited by their corresponding child process.

  if (!CreatePipe(read, write, &security_attributes, 0)) {
    return REPROC_ERROR_SYSTEM;
  }

  REPROC_ERROR error = REPROC_SUCCESS;

  if (!inherit_read) {
    error = pipe_disable_inherit(*read);
    if (error) {
      goto cleanup;
    }
  }

  if (!inherit_write) {
    error = pipe_disable_inherit(*write);
    if (error) {
      goto cleanup;
    }
  }

cleanup:
  if (error) {
    handle_close(read);
    handle_close(write);
  }

  return error;
}

REPROC_ERROR pipe_read(HANDLE pipe,
                       uint8_t *buffer,
                       unsigned int size,
                       unsigned int *bytes_read)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_read);

  // The cast is safe since `DWORD` is a typedef to `unsigned int` on Windows.
  if (!ReadFile(pipe, buffer, size, (LPDWORD) bytes_read, NULL)) {
    switch (GetLastError()) {
      case ERROR_BROKEN_PIPE:
        return REPROC_ERROR_STREAM_CLOSED;
      default:
        return REPROC_ERROR_SYSTEM;
    }
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR pipe_write(HANDLE pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_written);

  // The cast is safe since `DWORD` is a typedef to `unsigned int` on Windows.
  if (!WriteFile(pipe, buffer, size, (LPDWORD) bytes_written, NULL)) {
    switch (GetLastError()) {
      case ERROR_BROKEN_PIPE:
        return REPROC_ERROR_STREAM_CLOSED;
      default:
        return REPROC_ERROR_SYSTEM;
    }
  }

  if (*bytes_written != size) {
    return REPROC_ERROR_PARTIAL_WRITE;
  }

  return REPROC_SUCCESS;
}

#include <windows/pipe.h>

#include <macro.h>
#include <windows/handle.h>

#include <assert.h>
#include <stdio.h>
#include <windows.h>

#define PIPE_BUFFER_SIZE 65536
#define PIPE_SINGLE_INSTANCE 1
#define PIPE_NO_TIMEOUT 0

#define FILE_NO_SHARE 0
#define FILE_NO_TEMPLATE NULL

#ifdef __MINGW32__
static __thread unsigned long pipe_serial_number = 0;
#else
__declspec(thread) static unsigned long pipe_serial_number = 0;
#endif

REPROC_ERROR
pipe_init(HANDLE *read,
          struct pipe_options read_options,
          HANDLE *write,
          struct pipe_options write_options)
{
  // Windows anonymous pipes don't support overlapped I/O so we use named pipes
  // instead. This code has been adapted from
  // https://stackoverflow.com/a/419736/11900641.
  //
  // Copyright (c) 1997  Microsoft Corporation

  // Thread-safe unique name for the named pipe.
  char name[MAX_PATH];
  sprintf(name, "\\\\.\\Pipe\\RemoteExeAnon.%08lx.%08lx.%08lx",
          GetCurrentProcessId(), GetCurrentThreadId(), pipe_serial_number++);

  DWORD read_mode = read_options.overlapped ? FILE_FLAG_OVERLAPPED : 0;
  DWORD write_mode = write_options.overlapped ? FILE_FLAG_OVERLAPPED : 0;

  SECURITY_ATTRIBUTES security = { .nLength = sizeof(SECURITY_ATTRIBUTES),
                                   .lpSecurityDescriptor = NULL };

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  security.bInheritHandle = read_options.inherit;

  *read = CreateNamedPipeA(name, PIPE_ACCESS_INBOUND | read_mode,
                           PIPE_TYPE_BYTE | PIPE_WAIT, PIPE_SINGLE_INSTANCE,
                           PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, PIPE_NO_TIMEOUT,
                           &security);

  if (*read == INVALID_HANDLE_VALUE) {
    goto cleanup;
  }

  security.bInheritHandle = write_options.inherit;

  *write = CreateFileA(name, GENERIC_WRITE, FILE_NO_SHARE, &security,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | write_mode,
                       FILE_NO_TEMPLATE);

  if (*write == INVALID_HANDLE_VALUE) {
    goto cleanup;
  }

  error = REPROC_SUCCESS;

cleanup:
  if (error) {
    handle_close(read);
    handle_close(write);
  }

  return error;
}

static SECURITY_ATTRIBUTES DO_NOT_INHERIT = { .nLength = sizeof(
                                                  SECURITY_ATTRIBUTES),
                                              .bInheritHandle = false,
                                              .lpSecurityDescriptor = NULL };

REPROC_ERROR pipe_read(HANDLE pipe,
                       uint8_t *buffer,
                       unsigned int size,
                       unsigned int *bytes_read)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_read);

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  OVERLAPPED overlapped = { 0 };
  overlapped.hEvent = CreateEvent(&DO_NOT_INHERIT, true, false, NULL);
  if (overlapped.hEvent == NULL) {
    goto cleanup;
  }

  BOOL rv = ReadFile(pipe, buffer, size, NULL, &overlapped);
  if (rv == 0) {
    switch (GetLastError()) {
      case ERROR_BROKEN_PIPE:
        error = REPROC_ERROR_STREAM_CLOSED;
        goto cleanup;
      case ERROR_IO_PENDING:
        break;
      default:
        goto cleanup;
    }
  }

  // The cast is safe since `DWORD` is a typedef to `unsigned int` on Windows.
  rv = GetOverlappedResult(pipe, &overlapped, (LPDWORD) bytes_read, TRUE);
  if (rv == 0) {
    goto cleanup;
  }

  error = REPROC_SUCCESS;

cleanup:
  handle_close(&overlapped.hEvent);

  return error;
}

REPROC_ERROR pipe_write(HANDLE pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written)
{
  assert(pipe);
  assert(buffer);
  assert(bytes_written);

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  OVERLAPPED overlapped = { 0 };
  overlapped.hEvent = CreateEvent(&DO_NOT_INHERIT, true, false, NULL);
  if (overlapped.hEvent == NULL) {
    goto cleanup;
  }

  BOOL rv = WriteFile(pipe, buffer, size, NULL, &overlapped);
  if (rv == 0) {
    switch (GetLastError()) {
      case ERROR_BROKEN_PIPE:
        error = REPROC_ERROR_STREAM_CLOSED;
        goto cleanup;
      case ERROR_IO_PENDING:
        break;
      default:
        goto cleanup;
    }
  }

  // The cast is safe since `DWORD` is a typedef to `unsigned int` on Windows.
  rv = GetOverlappedResult(pipe, &overlapped, (LPDWORD) bytes_written, TRUE);
  if (rv == 0) {
    goto cleanup;
  }

  error = REPROC_SUCCESS;

cleanup:
  handle_close(&overlapped.hEvent);

  return error;
}

REPROC_ERROR pipe_wait(HANDLE *ready, HANDLE out, HANDLE err)
{
  assert(ready);

  HANDLE handles[2] = { err, out };
  OVERLAPPED overlapped[2] = { 0 };
  HANDLE events[2] = { 0 };
  DWORD size = 0;

  // `BOOL` is either 0 or 1 even though it is defined as `int` so all casts of
  // functions returning `BOOL` to `DWORD` in this function are safe.
  DWORD rv = 0;
  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  // We emulate POSIX `poll` by issuing overlapped zero-sized reads and waiting
  // for the first one to complete. This approach is inspired by the CPython
  // multiprocessing module `wait` implementation:
  // https://github.com/python/cpython/blob/10ecbadb799ddf3393d1fc80119a3db14724d381/Lib/multiprocessing/connection.py#L826

  for (DWORD i = 0; i < ARRAY_SIZE(handles); i++) {
    overlapped[i].hEvent = CreateEvent(&DO_NOT_INHERIT, true, false, NULL);
    if (overlapped[i].hEvent == NULL) {
      goto cleanup;
    }

    rv = (DWORD) ReadFile(handles[i], (uint8_t[]){ 0 }, 0, NULL,
                          &overlapped[i]);
    if (rv == 0 && GetLastError() != ERROR_IO_PENDING &&
        GetLastError() != ERROR_BROKEN_PIPE) {
      goto cleanup;
    }

    if (rv != 0 || GetLastError() == ERROR_IO_PENDING) {
      events[size++] = overlapped[i].hEvent;
    }
  }

  if (size == 0) {
    error = REPROC_ERROR_STREAM_CLOSED;
    goto cleanup;
  }

  rv = WaitForMultipleObjects(size, events, false, INFINITE);

  // We don't expect `WAIT_ABANDONED_0` or `WAIT_TIMEOUT` to be valid here.
  assert(rv < WAIT_ABANDONED_0);
  assert(rv != WAIT_TIMEOUT);

  if (rv == WAIT_FAILED) {
    goto cleanup;
  }

  assert(rv < size);

  // Map the signaled event back to its corresponding handle.
  *ready = events[rv] == overlapped[0].hEvent ? handles[0] : handles[1];

  error = REPROC_SUCCESS;

cleanup:;
  // Because we continue cleaning up even when an error occurs during cleanup,
  // we have to save the first error that occurs because `GetLastError()` might
  // get overridden with another error during further cleanup. When we're done,
  // we reset `GetLastError()` to the first error that occurred that we couldn't
  // handle.
  DWORD first_system_error = 0;

  for (DWORD i = 0; i < ARRAY_SIZE(handles); i++) {

    // Cancel any remaining zero-sized reads that we queued if they have not yet
    // completed.

    rv = (DWORD) CancelIoEx(handles[i], &overlapped[i]);
    if (rv == 0) {
      if (GetLastError() != ERROR_NOT_FOUND) {
        error = REPROC_ERROR_SYSTEM;

        if (!first_system_error) {
          first_system_error = GetLastError();
        }
      }

      continue;
    }

    // `CancelIoEx` only requests cancellation. We use `GetOverlappedResult` to
    // wait until the read is actually cancelled.

    DWORD bytes_transferred = 0;
    rv = (DWORD) GetOverlappedResult(handles[i], &overlapped[i],
                                     &bytes_transferred, TRUE);
    if (rv == 0 && GetLastError() != ERROR_OPERATION_ABORTED &&
        GetLastError() != ERROR_BROKEN_PIPE) {
      error = REPROC_ERROR_SYSTEM;

      if (!first_system_error) {
        first_system_error = GetLastError();
      }
    }
  }

  for (DWORD i = 0; i < ARRAY_SIZE(handles); i++) {
    handle_close(&overlapped[i].hEvent);
  }

  if (first_system_error) {
    SetLastError(first_system_error);
  }

  return error;
}

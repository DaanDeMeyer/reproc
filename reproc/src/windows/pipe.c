#include "pipe.h"

#include "error.h"
#include "macro.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

enum {
  PIPE_BUFFER_SIZE = 65536,
  PIPE_SINGLE_INSTANCE = 1,
  PIPE_NO_TIMEOUT = 0,
  FILE_NO_SHARE = 0,
  FILE_NO_TEMPLATE = 0
};

static SECURITY_ATTRIBUTES HANDLE_DO_NOT_INHERIT = {
  .nLength = sizeof(SECURITY_ATTRIBUTES),
  .bInheritHandle = false,
  .lpSecurityDescriptor = NULL
};

static THREAD_LOCAL unsigned long pipe_serial_number = 0;

int pipe_init(HANDLE *read,
              struct pipe_options read_options,
              HANDLE *write,
              struct pipe_options write_options)
{
  assert(read);
  assert(write);

  // Windows anonymous pipes don't support overlapped I/O so we use named pipes
  // instead. This code has been adapted from
  // https://stackoverflow.com/a/419736/11900641.
  //
  // Copyright (c) 1997  Microsoft Corporation

  char name[MAX_PATH]; // Thread-safe unique name for the named pipe.
  HANDLE pipe_handles[2] = { HANDLE_INVALID, HANDLE_INVALID };
  DWORD read_mode = read_options.nonblocking ? FILE_FLAG_OVERLAPPED : 0;
  DWORD write_mode = write_options.nonblocking ? FILE_FLAG_OVERLAPPED : 0;
  SECURITY_ATTRIBUTES security = { .nLength = sizeof(SECURITY_ATTRIBUTES),
                                   .lpSecurityDescriptor = NULL,
                                   .bInheritHandle = read_options.inherit };
  int r = 0;

  sprintf(name, "\\\\.\\Pipe\\RemoteExeAnon.%08lx.%08lx.%08lx",
          GetCurrentProcessId(), GetCurrentThreadId(), pipe_serial_number++);

  pipe_handles[0] = CreateNamedPipeA(name, PIPE_ACCESS_INBOUND | read_mode,
                                     PIPE_TYPE_BYTE | PIPE_WAIT,
                                     PIPE_SINGLE_INSTANCE, PIPE_BUFFER_SIZE,
                                     PIPE_BUFFER_SIZE, PIPE_NO_TIMEOUT,
                                     &security);
  if (pipe_handles[0] == INVALID_HANDLE_VALUE) {
    goto cleanup;
  }

  security.bInheritHandle = write_options.inherit;

  pipe_handles[1] = CreateFileA(name, GENERIC_WRITE, FILE_NO_SHARE, &security,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | write_mode,
                                (HANDLE) FILE_NO_TEMPLATE);
  if (pipe_handles[1] == INVALID_HANDLE_VALUE) {
    goto cleanup;
  }

  *read = pipe_handles[0];
  *write = pipe_handles[1];

  r = 1;

cleanup:
  if (r == 0) {
    handle_destroy(pipe_handles[0]);
    handle_destroy(pipe_handles[1]);
  }

  return error_unify(r);
}

int pipe_read(HANDLE pipe, uint8_t *buffer, size_t size)
{
  assert(pipe && pipe != HANDLE_INVALID);
  assert(buffer);
  assert(size <= UINT_MAX);

  DWORD bytes_read = 0;
  int r = 0;

  OVERLAPPED overlapped = { 0 };
  overlapped.hEvent = CreateEvent(&HANDLE_DO_NOT_INHERIT, true, false, NULL);
  if (overlapped.hEvent == NULL) {
    goto cleanup;
  }

  r = ReadFile(pipe, buffer, (DWORD) size, NULL, &overlapped);
  if (r == 0 && GetLastError() != ERROR_IO_PENDING) {
    goto cleanup;
  }

  r = GetOverlappedResult(pipe, &overlapped, &bytes_read, true);
  if (r == 0) {
    goto cleanup;
  }

  assert(bytes_read <= INT_MAX);

cleanup:
  handle_destroy(overlapped.hEvent);

  return error_unify_or_else(r, (int) bytes_read);
}

int pipe_write(HANDLE pipe, const uint8_t *buffer, size_t size)
{
  assert(pipe && pipe != HANDLE_INVALID);
  assert(buffer);
  assert(size <= UINT_MAX);

  DWORD bytes_written = 0;
  int r = 0;

  OVERLAPPED overlapped = { 0 };
  overlapped.hEvent = CreateEvent(&HANDLE_DO_NOT_INHERIT, true, false, NULL);
  if (overlapped.hEvent == NULL) {
    goto cleanup;
  }

  r = WriteFile(pipe, buffer, (DWORD) size, NULL, &overlapped);
  if (r == 0 && GetLastError() != ERROR_IO_PENDING) {
    goto cleanup;
  }

  r = GetOverlappedResult(pipe, &overlapped, &bytes_written, true);
  if (r == 0) {
    goto cleanup;
  }

  assert(bytes_written <= INT_MAX);

cleanup:
  handle_destroy(overlapped.hEvent);

  return error_unify_or_else(r, (int) bytes_written);
}

int pipe_wait(HANDLE out, HANDLE err, HANDLE *ready)
{
  assert(out && out != HANDLE_INVALID);
  assert(err && err != HANDLE_INVALID);
  assert(ready);

  HANDLE pipes[2] = { out, err };
  OVERLAPPED overlapped[2] = { { 0 }, { 0 } };
  HANDLE events[2] = { HANDLE_INVALID, HANDLE_INVALID };
  int r = 0;

  // We emulate POSIX `poll` by issuing overlapped zero-sized reads and waiting
  // for the first one to complete. This approach is inspired by the CPython
  // multiprocessing module `wait` implementation:
  // https://github.com/python/cpython/blob/10ecbadb799ddf3393d1fc80119a3db14724d381/Lib/multiprocessing/connection.py#L826

  for (size_t i = 0; i < ARRAY_SIZE(overlapped); i++) {
    overlapped[i].hEvent = CreateEvent(&HANDLE_DO_NOT_INHERIT, true, false,
                                       NULL);
    if (overlapped[i].hEvent == NULL) {
      r = 0;
      goto cleanup;
    }

    r = ReadFile(pipes[i], (uint8_t[]){ 0 }, 0, NULL, &overlapped[i]);
    if (r != 0 || GetLastError() == ERROR_BROKEN_PIPE) {
      // The read succeeded or we got a broken pipe error. Either way, we know
      // the pipe is ready to be processed further.
      r = 1;
      *ready = pipes[i];
      goto cleanup;
    }

    if (GetLastError() != ERROR_IO_PENDING) {
      goto cleanup;
    }

    events[i] = overlapped[i].hEvent;
  }

  DWORD result = WaitForMultipleObjects(ARRAY_SIZE(events), events, false,
                                        INFINITE);
  // We don't expect `WAIT_ABANDONED_0` or `WAIT_TIMEOUT` to be valid here.
  assert(result < WAIT_ABANDONED_0);
  assert(result != WAIT_TIMEOUT);

  if (result == WAIT_FAILED) {
    r = 0;
    goto cleanup;
  }

  // Map the signaled event back to its corresponding handle.
  *ready = pipes[result];
  r = 1;

cleanup:;
  PROTECT_SYSTEM_ERROR;

  for (size_t i = 0; i < ARRAY_SIZE(overlapped); i++) {

    // Cancel any remaining zero-sized reads that we queued if they have not yet
    // completed.

    r = CancelIoEx(pipes[i], &overlapped[i]);
    assert_unused(r != 0 || GetLastError() == ERROR_NOT_FOUND);

    if (r == 0 && GetLastError() == ERROR_NOT_FOUND) {
      continue;
    }

    // `CancelIoEx` only requests cancellation. We use `GetOverlappedResult` to
    // wait until the read is actually cancelled.

    DWORD bytes_transferred = 0;
    r = GetOverlappedResult(pipes[i], &overlapped[i], &bytes_transferred, true);
    assert_unused(r != 0 || (GetLastError() == ERROR_OPERATION_ABORTED ||
                             GetLastError() == ERROR_BROKEN_PIPE));
  }

  UNPROTECT_SYSTEM_ERROR;

  for (size_t i = 0; i < ARRAY_SIZE(overlapped); i++) {
    handle_destroy(overlapped[i].hEvent);
  }

  return error_unify(r);
}

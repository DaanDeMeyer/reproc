#include "pipe.h"

#include "error.h"
#include "macro.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
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
    goto finish;
  }

  security.bInheritHandle = write_options.inherit;

  pipe_handles[1] = CreateFileA(name, GENERIC_WRITE, FILE_NO_SHARE, &security,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL | write_mode,
                                (HANDLE) FILE_NO_TEMPLATE);
  if (pipe_handles[1] == INVALID_HANDLE_VALUE) {
    goto finish;
  }

  *read = pipe_handles[0];
  *write = pipe_handles[1];

  r = 1;

finish:
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
  r = ReadFile(pipe, buffer, (DWORD) size, NULL, &overlapped);
  if (r == 0 && GetLastError() != ERROR_IO_PENDING) {
    return error_unify(r);
  }

  r = GetOverlappedResultEx(pipe, &overlapped, &bytes_read, INFINITE, false);
  if (r == 0) {
    return error_unify(r);
  }

  assert(bytes_read <= INT_MAX);

  return error_unify_or_else(r, (int) bytes_read);
}

int pipe_write(HANDLE pipe, const uint8_t *buffer, size_t size, int timeout)
{
  assert(pipe && pipe != HANDLE_INVALID);
  assert(buffer);
  assert(size <= UINT_MAX);

  DWORD bytes_written = 0;
  int r = 0;

  OVERLAPPED overlapped = { 0 };
  r = WriteFile(pipe, buffer, (DWORD) size, NULL, &overlapped);
  if (r == 0 && GetLastError() != ERROR_IO_PENDING) {
    return error_unify(r);
  }

  r = GetOverlappedResultEx(pipe, &overlapped, &bytes_written,
                            timeout < 0 ? INFINITE : (DWORD) timeout, false);
  if (r == 0 && GetLastError() != WAIT_TIMEOUT) {
    return error_unify(r);
  }

  if (r == 0) {
    // The timeout expired. Cancel the ongoing write.
    r = CancelIo(pipe);
    assert_unused(r != 0);

    // Check if the write was actually cancelled.
    r = GetOverlappedResult(pipe, &overlapped, &bytes_written, true);

    // The write might have completed before it was cancelled so only error if
    // the operation was actually aborted.
    if (r == 0 && GetLastError() == ERROR_OPERATION_ABORTED) {
      r = -WAIT_TIMEOUT;
    }
  }

  assert(bytes_written <= INT_MAX);

  return error_unify_or_else(r, (int) bytes_written);
}

int pipe_wait(HANDLE out, HANDLE err, HANDLE *ready, int timeout)
{
  assert(ready);

  HANDLE pipes[2] = { HANDLE_INVALID, HANDLE_INVALID };
  DWORD num_pipes = 0;

  if (out != HANDLE_INVALID) {
    pipes[num_pipes++] = out;
  }

  if (err != HANDLE_INVALID) {
    pipes[num_pipes++] = err;
  }

  if (num_pipes == 0) {
    return -ERROR_BROKEN_PIPE;
  }

  OVERLAPPED overlapped[ARRAY_SIZE(pipes)] = { { 0 }, { 0 } };
  HANDLE events[ARRAY_SIZE(pipes)] = { HANDLE_INVALID, HANDLE_INVALID };
  int r = 0;

  // We emulate POSIX `poll` by issuing overlapped zero-sized reads and waiting
  // for the first one to complete. This approach is inspired by the CPython
  // multiprocessing module `wait` implementation:
  // https://github.com/python/cpython/blob/10ecbadb799ddf3393d1fc80119a3db14724d381/Lib/multiprocessing/connection.py#L826

  for (size_t i = 0; i < num_pipes; i++) {
    overlapped[i].hEvent = CreateEvent(&HANDLE_DO_NOT_INHERIT, true, false,
                                       NULL);
    if (overlapped[i].hEvent == NULL) {
      r = 0;
      goto finish;
    }

    r = ReadFile(pipes[i], (uint8_t[]){ 0 }, 0, NULL, &overlapped[i]);
    if (r != 0 || GetLastError() == ERROR_BROKEN_PIPE) {
      // The read succeeded or we got a broken pipe error. Either way, we know
      // the pipe is ready to be processed further.
      r = 1;
      *ready = pipes[i];
      goto finish;
    }

    if (GetLastError() != ERROR_IO_PENDING) {
      goto finish;
    }

    events[i] = overlapped[i].hEvent;
  }

  DWORD result = WaitForMultipleObjects(num_pipes, events, false,
                                        timeout < 0 ? INFINITE
                                                    : (DWORD) timeout);
  // We don't expect `WAIT_ABANDONED_0` to be valid here.
  assert(result != WAIT_ABANDONED_0);

  if (result == WAIT_TIMEOUT) {
    r = -WAIT_TIMEOUT;
    goto finish;
  }

  if (result == WAIT_FAILED) {
    r = 0;
    goto finish;
  }

  // Map the signaled event back to its corresponding handle.
  *ready = pipes[result];
  r = 1;

finish:
  for (size_t i = 0; i < num_pipes; i++) {
    // Cancel any remaining zero-sized reads that we queued if they have not yet
    // completed.

    if (events[i] == HANDLE_INVALID) {
      continue;
    }

    PROTECT_SYSTEM_ERROR;

    r = CancelIo(pipes[i]);
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

    UNPROTECT_SYSTEM_ERROR;
  }

  for (size_t i = 0; i < num_pipes; i++) {
    handle_destroy(overlapped[i].hEvent);
  }

  return error_unify(r);
}

#pragma once

#include <reproc/error.h>

#include <wchar.h>
#include <windows.h>

struct process_options {
  // If not `NULL`, the working directory of the child process is set to
  // `working_directory`.
  const wchar_t *working_directory;
  // if not `NULL`, the stdin, stdout and stderr of the child process are
  // redirected to `stdin_handle`, `stdout_handle` and `stderr_handle`
  // respectively.
  HANDLE stdin_handle;
  HANDLE stdout_handle;
  HANDLE stderr_handle;
};

// Escapes and joins the arguments in `argv` into a single string as expected by
// `CreateProcess`.
char *argv_join(const char *const *argv);

// Converts the UTF-8 string in `string` to a UTF-16 string.
wchar_t *string_to_wstring(const char *string);

// Spawns a child process that executes the command stored in `command_line`.
// The process id and handle of the new child process are assigned to `pid` and
// `handle` respectively. `options` contains options that modify how the child
// process is spawned. See `process_options` for more information on the
// possible options.
REPROC_ERROR process_create(wchar_t *command_line,
                            struct process_options *options,
                            DWORD *pid,
                            HANDLE *handle);

// Waits `timeout` milliseconds for the process indicated by `pid` to exit and
// stores the exit code in `exit_status`.
REPROC_ERROR
process_wait(HANDLE process, unsigned int timeout, unsigned int *exit_status);

// Sends the `CTRL-BREAK` signal to the process indicated by `pid`.
REPROC_ERROR process_terminate(unsigned long pid);

// Calls `TerminateProcess` on the process indicated by `handle`.
REPROC_ERROR process_kill(HANDLE process);

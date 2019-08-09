#pragma once

#include <reproc/error.h>

#include <stdbool.h>
#include <sys/types.h>

struct process_options {
  // If not `NULL`, the working directory of the child process is set to
  // `working_directory`.
  const char *working_directory;
  // if not 0, the stdin, stdout and stderr of the child process are redirected
  // to `stdin_fd`, `stdout_fd` and `stderr_fd` respectively.
  int stdin_fd;
  int stdout_fd;
  int stderr_fd;
  // Don't wait for `action` to complete in the child process before returning
  // from `process_create`. Returning early also results in errors from `action`
  // not being reported.
  bool return_early;
  // Use `vfork` instead of `fork`. Using `vfork` results in the parent process
  // being suspended until the child process exits or calls any function from
  // the `exec` family of functions. If `vfork` is enabled, make sure any code
  // executed within `action` is within the constraints of `vfork`.
  bool vfork;
};

// Creates a child process and calls `action` with `context` in the child
// process. The process id of the new child process is assigned to `pid`.
// `options` contains options that modify how the child process is spawned. See
// `process_options` for more information on the possible options.
REPROC_ERROR
process_create(int (*action)(const void *),
               const void *context,
               struct process_options *options,
               pid_t *pid);

// Waits `timeout` milliseconds for the process indicated by `pid` to exit and
// stores the exit code in `exit_status`.
REPROC_ERROR
process_wait(pid_t pid, unsigned int timeout, unsigned int *exit_status);

// Sends the `SIGTERM` signal to the process indicated by `pid`.
REPROC_ERROR process_terminate(pid_t pid);

// Sends the `SIGKILL` signal to the process indicated by `pid`.
REPROC_ERROR process_kill(pid_t pid);

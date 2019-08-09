#pragma once

#include <reproc/error.h>

#include <stdbool.h>
#include <sys/types.h>

/*
Creates a child process and calls `execvp` with the arguments in `argv`. The
process id of the new child process is assigned to `pid`.

If not `NULL`, the working directory of the child process is set to
`working_directory`.

if not 0, the stdin, stdout and stderr of the child process are redirected
to `stdin_fd`, `stdout_fd` and `stderr_fd` respectively.
*/
REPROC_ERROR
process_create(const char *const *argv,
               const char *working_directory,
               int stdin_fd,
               int stdout_fd,
               int stderr_fd,
               pid_t *pid);

// Waits `timeout` milliseconds for the process indicated by `pid` to exit and
// stores the exit code in `exit_status`.
REPROC_ERROR
process_wait(pid_t pid, unsigned int timeout, unsigned int *exit_status);

// Sends the `SIGTERM` signal to the process indicated by `pid`.
REPROC_ERROR process_terminate(pid_t pid);

// Sends the `SIGKILL` signal to the process indicated by `pid`.
REPROC_ERROR process_kill(pid_t pid);

#pragma once

#include <reproc/error.h>

#include <stdbool.h>
#include <sys/types.h>

struct process_options {
  const char *const *environment;
  // If not `NULL`, the working directory of the child process is set to
  // `working_directory`.
  const char *working_directory;

  // The stdin, stdout and stderr of the child process are redirected to `in`,
  // `out` and `err` respectively.
  struct {
    int in;
    int out;
    int err;
  } redirect;
};

// Creates a child process and calls `execvp` with the arguments in `argv`. The
// process id of the new child process is assigned to `process`.
REPROC_ERROR
process_create(pid_t *process,
               const char *const *argv,
               struct process_options options);

// Waits `timeout` milliseconds for the process indicated by `pid` to exit and
// stores the exit code in `exit_status`.
REPROC_ERROR
process_wait(pid_t process, unsigned int timeout, unsigned int *exit_status);

// Sends the `SIGTERM` signal to the process indicated by `pid`.
REPROC_ERROR process_terminate(pid_t process);

// Sends the `SIGKILL` signal to the process indicated by `pid`.
REPROC_ERROR process_kill(pid_t process);

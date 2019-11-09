#pragma once

#include <reproc/error.h>

typedef void *HANDLE;

struct process_options {
  // If `NULL`, the child process inherits the environment of the current
  // process.
  const char *const *environment;
  // If not `NULL`, the working directory of the child process is set to
  // `working_directory`.
  const char *working_directory;

  struct {
    HANDLE in;
    HANDLE out;
    HANDLE err;
  } redirect;
};

// Spawns a child process that executes the command stored in `command_line`.
// `command_line` is required to be in the format expected by `CreateProcessW`.
// The process id and handle of the new child process are assigned to `pid` and
// `handle` respectively. `options` contains options that modify how the child
// process is spawned. See `process_options` for more information on the
// possible options.
REPROC_ERROR process_create(HANDLE *process,
                            const char *const *argv,
                            struct process_options options);

// Waits `timeout` milliseconds for the process indicated by `pid` to exit and
// stores the exit code in `exit_status`.
REPROC_ERROR
process_wait(HANDLE process, unsigned int timeout, unsigned int *exit_status);

// Sends the `CTRL-BREAK` signal to the process indicated by `pid`.
REPROC_ERROR process_terminate(HANDLE process);

// Calls `TerminateProcess` on the process indicated by `handle`.
REPROC_ERROR process_kill(HANDLE process);

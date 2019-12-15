#pragma once

#include <reproc/error.h>

#include <handle.h>

struct process_options {
  // If `NULL`, the child process inherits the environment of the current
  // process.
  const char *const *environment;
  // If not `NULL`, the working directory of the child process is set to
  // `working_directory`.
  const char *working_directory;

  struct {
    reproc_handle in;
    reproc_handle out;
    reproc_handle err;
  } redirect;
};

// Spawns a child process that executes the command stored in `argv`.
// The process handle of the new child process is assigned to `process`.
REPROC_ERROR process_create(reproc_handle *process,
                            const char *const *argv,
                            struct process_options options);

// Waits `timeout` milliseconds for any process in `processes` to exit. If a
// process exits within the configured timeout, its index in `processes` is
// stored in `completed` and its exit status is stored in `exit_status`.
//
// If `timeout` is `REPROC_INFINITE`, this function waits indefinitely for a
// process to exit.
REPROC_ERROR
process_wait(reproc_handle *processes,
             unsigned int num_processes,
             unsigned int timeout,
             unsigned int *completed,
             int *exit_status);

// Sends the `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) signal to the process
// indicated by `process`.
REPROC_ERROR process_terminate(reproc_handle process);

// Sends the `SIGKILL` signal to `process` (POSIX) or calls `TerminateProcess`
// on `process` (Windows).
REPROC_ERROR process_kill(reproc_handle process);

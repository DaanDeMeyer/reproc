#pragma once

#include <handle.h>

struct process_options {
  // If `NULL`, the child process inherits the environment of the current
  // process.
  const char *const *environment;
  // If not `NULL`, the working directory of the child process is set to
  // `working_directory`.
  const char *working_directory;

  struct {
    handle in;
    handle out;
    handle err;
  } redirect;
};

// Spawns a child process that executes the command stored in `argv`.
// The process handle of the new child process is assigned to `process`.
int process_create(handle *process,
                   const char *const *argv,
                   struct process_options options);

// Waits `timeout` milliseconds for any process in `processes` to exit. If a
// process exits within the configured timeout, its index in `processes` is
// returned and its exit status is stored in `exit_status`.
//
// If `timeout` is `REPROC_INFINITE`, this function waits indefinitely for a
// process to exit.
int process_wait(handle *processes,
                 unsigned int num_processes,
                 unsigned int timeout,
                 int *exit_status);

// Sends the `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) signal to the process
// indicated by `process`.
int process_terminate(handle process);

// Sends the `SIGKILL` signal to `process` (POSIX) or calls `TerminateProcess`
// on `process` (Windows).
int process_kill(handle process);

handle process_destroy(handle process);

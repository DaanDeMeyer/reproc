#pragma once

#include "handle.h"

struct stdio {
  handle in;
  handle out;
  handle err;
};

struct process_options {
  // If `NULL`, the child process inherits the environment of the current
  // process.
  const char *const *environment;
  // If not `NULL`, the working directory of the child process is set to
  // `working_directory`.
  const char *working_directory;
  // The standard streams of the child process are redirected to the handles in
  // `redirect`. If a handle is `HANDLE_INVALID`, the corresponding child
  // process standard stream is closed.
  struct stdio redirect;
};

// Spawns a child process that executes the command stored in `argv`.
// The process handle of the new child process is assigned to `process`.
int process_start(handle *process,
                  const char *const *argv,
                  struct process_options options);

// Waits `timeout` milliseconds for `process` to exit. If `process` exits within
// the configured timeout, its exit status is returned.
//
// If `timeout` is `REPROC_INFINITE`, this function waits indefinitely for a
// process to exit.
int process_wait(handle process, int timeout);

// Sends the `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) signal to the process
// indicated by `process`.
int process_terminate(handle process);

// Sends the `SIGKILL` signal to `process` (POSIX) or calls `TerminateProcess`
// on `process` (Windows).
int process_kill(handle process);

handle process_destroy(handle process);

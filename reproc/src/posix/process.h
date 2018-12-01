#ifndef REPROC_POSIX_PROCESS_H
#define REPROC_POSIX_PROCESS_H

#include <reproc/error.h>

#include <stdbool.h>
#include <sys/types.h>

struct process_options {
  // Set the working directory of the child process to `working_directory` if it
  // is not `NULL`.
  const char *working_directory;
  // Redirect stdin, stdout and stderr to `stdin_fd`, `stdout_fd` and
  // `stderr_fd` respectively if not zero.
  int stdin_fd;
  int stdout_fd;
  int stderr_fd;
  // `process_group` is passed directly to `setpgid`'s second argument (passing
  // 0 will create a new process group with the same value as the new child
  // process' pid).
  pid_t process_group;
  // Don't wait for `action` to complete in the child process before returning
  // from `process_create`. Returning early also results in errors from `action`
  // not being reported.
  bool return_early;
  bool vfork;
};

/* Creates a child process and calls `action` with `context` in the child
process. The process id of the new child process is assigned to `pid`. If
`vfork` is enabled, make sure any code executed within `action` is within the
constraints of `vfork`. */
REPROC_ERROR
process_create(int (*action)(const void *), const void *context,
               struct process_options *options, pid_t *pid);

REPROC_ERROR process_wait(pid_t pid, unsigned int timeout,
                          unsigned int *exit_status);

REPROC_ERROR process_terminate(pid_t pid);

REPROC_ERROR process_kill(pid_t pid);

#endif

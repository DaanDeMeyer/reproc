#ifndef REPROC_C_POSIX_FORK_H
#define REPROC_C_POSIX_FORK_H

#include "reproc/error.h"

#include <stdbool.h>
#include <sys/types.h>

struct fork_options {
  // Set the working directory of the child process to working_directory if not
  // NULL.
  const char *working_directory;
  // Redirect stdin, stdout and stderr to stdin_fd, stdout_fd and stderr_fd
  // respectively if not zero.
  int stdin_fd;
  int stdout_fd;
  int stderr_fd;
  // process_group is passed directly to set_pgid's second argument (passing 0
  // will create a new process group with the same value as the new child
  // process' pid).
  pid_t process_group;
  // Don't wait for action to complete in the child process before returning
  // from fork_action. Returning early also results in errors from action not
  // being reported.
  bool return_early;
};

/*
Forks child process and calls action with data in the forked child process.

Process id of the new child process is assigned to pid.
*/
REPROC_ERROR fork_action(int (*action)(const void *), const void *data,
                         struct fork_options *options, pid_t *pid);

#endif

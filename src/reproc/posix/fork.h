#ifndef REPROC_C_POSIX_FORK_H
#define REPROC_C_POSIX_FORK_H

#include "reproc/error.h"

#include <sys/types.h>

struct fork_options {
  const char *working_directory;
  int stdin_fd;
  int stdout_fd;
  int stderr_fd;
  pid_t process_group;
};

/*
Forks child process and calls action with data in the forked child process.

Sets the working directory of the child process to working_directory if not
NULL.

Redirects stdin, stdout and stderr to stdin_fd, stdout_fd and stderr_fd
respectively if zero isn't passed.

process_group is passed directly to set_pgid's second argument (passing 0 will
create a new process group with the same value as the new child process' pid).

Process id of the new child process is assigned to pid.
*/
REPROC_ERROR fork_action(int (*action)(const void *), const void *data,
                         struct fork_options *options, pid_t *pid);

#endif

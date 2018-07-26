#ifndef REPROC_C_POSIX_FORK_H
#define REPROC_C_POSIX_FORK_H

#include "reproc/error.h"

#include <sys/types.h>

REPROC_ERROR fork_exec_redirect(int argc, const char *const *argv,
                                const char *working_directory, int stdin_pipe,
                                int stdout_pipe, int stderr_pipe, pid_t *pid);

REPROC_ERROR fork_timeout(unsigned int milliseconds, pid_t process_group,
                          pid_t *pid);

#endif

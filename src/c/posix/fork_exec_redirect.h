#ifndef REPROC_C_POSIX_FORK_EXEC_REDIRECT_H
#define REPROC_C_POSIX_FORK_EXEC_REDIRECT_H

#include "reproc/reproc.h"

#include <sys/types.h>

REPROC_ERROR fork_exec_redirect(int argc, const char *argv[],
                                const char *working_directory, int stdin_pipe,
                                int stdout_pipe, int stderr_pipe,
                                pid_t *pid);

#endif

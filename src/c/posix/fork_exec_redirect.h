#pragma once

#include "reproc/reproc.h"

#include <sys/types.h>

REPROC_ERROR fork_exec_redirect(int argc, const char *argv[],
                                     const char *working_directory,
                                     int stdin_pipe, int stdout_pipe,
                                     int stderr_pipe, pid_t *pid);

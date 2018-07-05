#pragma once

#include "process.h"

#include <sys/types.h>

PROCESS_LIB_ERROR fork_exec_redirect(int argc, const char *argv[],
                                     const char *working_directory,
                                     int stdin_pipe, int stdout_pipe,
                                     int stderr_pipe, pid_t *pid);





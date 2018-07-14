#pragma once

#include "process-lib/process.h"

#include <sys/types.h>

PROCESS_LIB_ERROR wait_no_hang(pid_t pid, int *exit_status);

PROCESS_LIB_ERROR wait_infinite(pid_t pid, int *exit_status);

PROCESS_LIB_ERROR wait_timeout(pid_t pid, int *exit_status,
                               unsigned int milliseconds);

int parse_exit_status(int status);

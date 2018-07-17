#pragma once

#include "reproc/reproc.h"

#include <sys/types.h>

REPROC_ERROR wait_no_hang(pid_t pid, int *exit_status);

REPROC_ERROR wait_infinite(pid_t pid, int *exit_status);

REPROC_ERROR wait_timeout(pid_t pid, int *exit_status,
                               unsigned int milliseconds);

int parse_exit_status(int status);

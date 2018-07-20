#ifndef REPROC_C_POSIX_WAIT_H
#define REPROC_C_POSIX_WAIT_H

#include "reproc/reproc.h"

#include <sys/types.h>

REPROC_ERROR wait_no_hang(pid_t pid, int *exit_status);

REPROC_ERROR wait_infinite(pid_t pid, int *exit_status);

REPROC_ERROR wait_timeout(pid_t pid, int *exit_status,
                          unsigned int milliseconds);

#endif

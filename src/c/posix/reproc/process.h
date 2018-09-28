#ifndef REPROC_C_POSIX_WAIT_H
#define REPROC_C_POSIX_WAIT_H

#include "reproc/error.h"

#include <sys/types.h>

REPROC_ERROR process_wait(pid_t pid, unsigned int timeout,
                          unsigned int *exit_status);

REPROC_ERROR process_terminate(pid_t pid, unsigned int timeout,
                               unsigned int *exit_status);

REPROC_ERROR process_kill(pid_t pid, unsigned int timeout,
                          unsigned int *exit_status);

#endif

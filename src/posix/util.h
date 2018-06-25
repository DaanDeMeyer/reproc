#pragma once

#include "process.h"

#include <stdint.h>
#include <sys/types.h>

/* Creates a pipe with pipe() and if the call succeeds assign the read endpoint
 * of the pipe to read and the write endpoint to write.
 */
PROCESS_LIB_ERROR pipe_init(int *read, int *write);

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer, uint32_t to_write,
                             uint32_t *actual);

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, uint32_t to_read,
                            uint32_t *actual);

PROCESS_LIB_ERROR pipe_close(int *pipe_address);

PROCESS_LIB_ERROR wait_no_hang(pid_t pid, int *exit_status);

PROCESS_LIB_ERROR wait_infinite(pid_t pid, int *exit_status);

PROCESS_LIB_ERROR wait_timeout(pid_t pid, int *exit_status,
                               uint32_t milliseconds);

int32_t parse_exit_status(int status);

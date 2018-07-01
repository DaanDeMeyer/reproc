#pragma once

#include "process.h"

#include <sys/types.h>

PROCESS_LIB_ERROR process_spawn(const char *argv[],
                                 const char *working_directory, int stdin_pipe,
                                 int stdout_pipe, int stderr_pipe, pid_t *pid);

PROCESS_LIB_ERROR pipe_init(int *read, int *write);

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer,
                             unsigned int to_write, unsigned int *actual);

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, unsigned int to_read,
                            unsigned int *actual);

PROCESS_LIB_ERROR pipe_close(int *pipe_address);

PROCESS_LIB_ERROR wait_no_hang(pid_t pid, int *exit_status);

PROCESS_LIB_ERROR wait_infinite(pid_t pid, int *exit_status);

PROCESS_LIB_ERROR wait_timeout(pid_t pid, int *exit_status,
                               unsigned int milliseconds);

int parse_exit_status(int status);

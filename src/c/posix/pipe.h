#pragma once

#include "process.h"

PROCESS_LIB_ERROR pipe_init(int *read, int *write);

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer,
                             unsigned int to_write, unsigned int *actual);

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, unsigned int size,
                            unsigned int *actual);

void pipe_close(int *pipe_address);

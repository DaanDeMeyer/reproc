#pragma once

#include "process-lib/process.h"

PROCESS_LIB_ERROR pipe_init(int *read, int *write);

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer,
                             unsigned int to_write,
                             unsigned int *bytes_written);

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, unsigned int size,
                            unsigned int *bytes_read);

void pipe_close(int *pipe_address);

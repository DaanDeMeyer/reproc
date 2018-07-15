#pragma once

#include "process-lib/process.h"

#include <windows.h>

PROCESS_LIB_ERROR pipe_init(HANDLE *read, HANDLE *write);

PROCESS_LIB_ERROR pipe_disable_inherit(HANDLE pipe);

PROCESS_LIB_ERROR pipe_write(HANDLE pipe, const void *buffer,
                             unsigned int to_write, unsigned int *bytes_written);

PROCESS_LIB_ERROR pipe_read(HANDLE pipe, void *buffer, unsigned int size,
                            unsigned int *bytes_read);

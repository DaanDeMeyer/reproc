#ifndef REPROC_WINDOWS_PIPE_H
#define REPROC_WINDOWS_PIPE_H

#include "reproc/error.h"

#include <windows.h>

REPROC_ERROR pipe_init(HANDLE *read, HANDLE *write);

REPROC_ERROR pipe_disable_inherit(HANDLE pipe);

REPROC_ERROR pipe_write(HANDLE pipe, const void *buffer, unsigned int to_write,
                        unsigned int *bytes_written);

REPROC_ERROR pipe_read(HANDLE pipe, void *buffer, unsigned int size,
                       unsigned int *bytes_read);

#endif

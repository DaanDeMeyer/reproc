#ifndef REPROC_WINDOWS_PIPE_H
#define REPROC_WINDOWS_PIPE_H

#include <reproc/error.h>

#include <stdbool.h>
#include <windows.h>

REPROC_ERROR pipe_init(HANDLE *read, bool inherit_read, HANDLE *write,
                       bool inherit_write);

REPROC_ERROR pipe_read(HANDLE pipe, void *buffer, unsigned int size,
                       unsigned int *bytes_read);

REPROC_ERROR pipe_write(HANDLE pipe, const void *buffer, unsigned int to_write,
                        unsigned int *bytes_written);

#endif

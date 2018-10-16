#ifndef REPROC_C_POSIX_PIPE_H
#define REPROC_C_POSIX_PIPE_H

#include "reproc/error.h"

REPROC_ERROR pipe_init(int *read, int *write);

REPROC_ERROR pipe_write(int pipe, const void *buffer, unsigned int to_write,
                        unsigned int *bytes_written);

REPROC_ERROR pipe_read(int pipe, void *buffer, unsigned int size,
                       unsigned int *bytes_read);

void pipe_close(int *pipe);

#endif

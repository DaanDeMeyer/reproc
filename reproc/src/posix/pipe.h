#pragma once

#include <reproc/error.h>

#include <stdbool.h>
#include <stdint.h>

struct pipe_options {
  bool nonblocking;
};

// Creates a new anonymous pipe. `read` and `write` are set to the read and
// write end of the pipe respectively.
REPROC_ERROR pipe_init(int *read,
                       struct pipe_options read_options,
                       int *write,
                       struct pipe_options write_options);

// Reads up to `size` bytes from the pipe indicated by `pipe` and stores them
// them in `buffer`. The amount of bytes read is stored in `bytes_read`.
REPROC_ERROR
pipe_read(int pipe,
          uint8_t *buffer,
          unsigned int size,
          unsigned int *bytes_read);

// Writes up to `size` bytes from `buffer` to the pipe indicated by `pipe`
// and stores the amount of bytes written in `bytes_written`.
//
// For pipes, the `write` system call on POSIX platforms will block until the
// requested amount of bytes have been written to the pipe so this function
// should only rarely succeed without writing the full amount of bytes
// requested.
//
// By default, writing to a closed pipe terminates a process with the `SIGPIPE`
// signal. `pipe_write` will only return `REPROC_ERROR_STREAM_CLOSED` if this
// signal is ignored by the running process.
REPROC_ERROR pipe_write(int pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written);

REPROC_ERROR pipe_wait(int *ready, int out, int err);

#pragma once

#include <reproc/error.h>

#include <handle.h>

#include <stdbool.h>
#include <stdint.h>

struct pipe_options {
  bool inherit; // Ignored on POSIX.
  bool nonblocking;
};

// Creates a new anonymous pipe. `read` and `write` are set to the read and
// write handle of the pipe respectively. `inherit_read` and `inherit_write`
// specify whether the `read` or `write` handle respectively should be inherited
// by any child processes spawned by the current process.
REPROC_ERROR
pipe_init(handle *read,
          struct pipe_options read_options,
          handle *write,
          struct pipe_options write_options);

// Reads up to `size` bytes from the pipe indicated by `handle` and stores them
// them in `buffer`. The amount of bytes read is stored in `bytes_read`.
//
// This function only works on pipe handles opened with the
// `FILE_FLAG_OVERLAPPED` flag.
REPROC_ERROR pipe_read(handle pipe,
                       uint8_t *buffer,
                       unsigned int size,
                       unsigned int *bytes_read);

// Writes up to `size` bytes from `buffer` to the pipe indicated by `handle`
// and stores the amount of bytes written in `bytes_written`.
//
// This function only works on pipe handles opened with the
// `FILE_FLAG_OVERLAPPED` flag.
REPROC_ERROR pipe_write(handle pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written);

// Block until one of the pipes in `pipes` has data available to read and stores
// its index in `ready`.
//
// `REPROC_ERROR_STREAM_CLOSED` is returned if all pipes in `pipes` have been
// closed.
REPROC_ERROR
pipe_wait(const handle *pipes,
          unsigned int num_pipes,
          unsigned int *ready);

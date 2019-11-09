#pragma once

#include <reproc/reproc.h>

struct pipe_options {
  bool inherit; // Ignored on POSIX.
  bool nonblocking;
};

// Creates a new anonymous pipe. `read` and `write` are set to the read and
// write handle of the pipe respectively. `inherit_read` and `inherit_write`
// specify whether the `read` or `write` handle respectively should be inherited
// by any child processes spawned by the current process.
REPROC_ERROR
pipe_init(reproc_handle *read,
          struct pipe_options read_options,
          reproc_handle *write,
          struct pipe_options write_options);

// Reads up to `size` bytes from the pipe indicated by `handle` and stores them
// them in `buffer`. The amount of bytes read is stored in `bytes_read`.
//
// This function only works on pipe handles opened with the
// `FILE_FLAG_OVERLAPPED` flag.
REPROC_ERROR pipe_read(reproc_handle pipe,
                       uint8_t *buffer,
                       unsigned int size,
                       unsigned int *bytes_read);

// Writes up to `size` bytes from `buffer` to the pipe indicated by `handle`
// and stores the amount of bytes written in `bytes_written`.
//
// This function only works on pipe handles opened with the
// `FILE_FLAG_OVERLAPPED` flag.
REPROC_ERROR pipe_write(reproc_handle pipe,
                        const uint8_t *buffer,
                        unsigned int size,
                        unsigned int *bytes_written);

// Block until `out` or `err` has data available to read. The first file
// descriptor that has data available to read is stored in `ready`.
//
// `REPROC_ERROR_STREAM_CLOSED` is returned if both `out` and `err` have been
// closed.
REPROC_ERROR
pipe_wait(reproc_handle *ready, reproc_handle out, reproc_handle err);

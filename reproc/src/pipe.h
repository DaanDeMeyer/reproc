#pragma once

#include "handle.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pipe_options {
  bool inherit; // Ignored on POSIX.
  bool nonblocking;
};

// Creates a new anonymous pipe. `read` and `write` are set to the read and
// write handle of the pipe respectively.
int pipe_init(handle *read,
              struct pipe_options read_options,
              handle *write,
              struct pipe_options write_options);

// Reads up to `size` bytes into `buffer` from the pipe indicated by `handle`
// and returns the amount of bytes read.
int pipe_read(handle pipe, uint8_t *buffer, size_t size);

// Writes up to `size` bytes from `buffer` to the pipe indicated by `handle`
// and returns the amount of bytes written.
int pipe_write(handle pipe, const uint8_t *buffer, size_t size);

// Block until one of the pipes in `pipes` has data available to read and return
// its index in `pipes`.
//
// `REPROC_ERROR_STREAM_CLOSED` is returned if all pipes in `pipes` have been
// closed.
int pipe_wait(const handle *pipes, size_t num_pipes);

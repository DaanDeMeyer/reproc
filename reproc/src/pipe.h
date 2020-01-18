#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
typedef void *pipe_type; // `SOCKET`
#else
typedef int pipe_type; // fd
#endif

extern const pipe_type PIPE_INVALID; // NOLINT

struct pipe_options {
  bool inherit; // Ignored on POSIX.
  bool nonblocking;
};

// Creates a new anonymous pipe. `read` and `write` are set to the read and
// write handle of the pipe respectively.
int pipe_init(pipe_type *read,
              struct pipe_options read_options,
              pipe_type *write,
              struct pipe_options write_options);

// Reads up to `size` bytes into `buffer` from the pipe indicated by `pipe` and
// returns the amount of bytes read.
int pipe_read(pipe_type pipe, uint8_t *buffer, size_t size);

// Writes up to `size` bytes from `buffer` to the pipe indicated by `pipe` and
// returns the amount of bytes written.
int pipe_write(pipe_type pipe, const uint8_t *buffer, size_t size, int timeout);

// Stores the value of the first of `out` and `ready` that will have data
// available to read in `ready`.
//
// Returns `REPROC_EPIPE` if both `out` and `err` are invalid handles.
int pipe_wait(pipe_type out, pipe_type err, pipe_type *ready, int timeout);

pipe_type pipe_destroy(pipe_type pipe);

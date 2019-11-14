#pragma once

// `reproc_handle` allows to define a cross-platform internal API that is
// implemented on each platform.
#if defined(_WIN32)
// `void *` = `HANDLE`
typedef void *reproc_handle;
#else
// `int` = `pid_t` (used for fd's as well)
typedef int reproc_handle;
#endif

// If `handle` is not `NULL`, closes `handle` and sets `handle` to `NULL`.
// Otherwise, does nothing. Does not overwrite the last system error if an error
// occurs while closing `handle`.
void handle_close(reproc_handle *handle);

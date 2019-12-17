#pragma once

// `handle` allows to define a cross-platform internal API that is
// implemented on each platform.
#if defined(_WIN32)
// `void *` = `HANDLE`
typedef void *handle;
#else
// `int` = `pid_t` (used for fd's as well)
typedef int handle;
#endif

extern const handle HANDLE_INVALID; // NOLINT

// Closes `handle` if it is not an invalid handle and returns an invalid handle.
// Does not overwrite the last system error if an error occurs while closing
// `handle`.
handle handle_destroy(handle handle);

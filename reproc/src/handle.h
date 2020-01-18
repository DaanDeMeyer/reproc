#pragma once

#if defined(_WIN32)
typedef void *handle_type; // `HANDLE`
#else
typedef int handle_type; // fd
#endif

extern const handle_type HANDLE_INVALID; // NOLINT

// Closes `handle` if it is not an invalid handle and returns an invalid handle.
// Does not overwrite the last system error if an error occurs while closing
// `handle`.
handle_type handle_destroy(handle_type handle);

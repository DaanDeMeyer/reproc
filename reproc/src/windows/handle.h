#ifndef REPROC_WINDOWS_HANDLE_H
#define REPROC_WINDOWS_HANDLE_H

#include <windows.h>

// If `handle` is not `NULL`, closes `handle` and sets `handle` to `NULL`.
// Otherwise, does nothing. Does not overwrite the result of `GetLastError` if
// an error occurs while closing `handle`.
void handle_close(HANDLE *handle);

#endif

#pragma once

#include <reproc/error.h>

#include <windows.h>

REPROC_ERROR handle_disable_inherit(HANDLE pipe);

// If `handle` is not `NULL`, closes `handle` and sets `handle` to `NULL`.
// Otherwise, does nothing. Does not overwrite the result of `GetLastError` if
// an error occurs while closing `handle`.
void handle_close(HANDLE *handle);

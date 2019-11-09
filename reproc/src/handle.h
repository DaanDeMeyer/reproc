#pragma once

#include <reproc/reproc.h>

// If `handle` is not `NULL`, closes `handle` and sets `handle` to `NULL`.
// Otherwise, does nothing. Does not overwrite the last system error if an error
// occurs while closing `handle`.
void handle_close(reproc_handle *handle);

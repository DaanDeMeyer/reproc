#pragma once

#include <reproc/error.h>

#include <wchar.h>

// Joins all the strings in `string_array` together delimited by spaces and
// stores the result in `result`.
//
// Possible errors:
// - `REPROC_NOT_ENOUGH_MEMORY`
REPROC_ERROR string_join(const char *const *string_array, int array_length,
                         char **result);

// Converts the UTF-8 string in `string` to a UTF-16 string and stores the
// result in `result`.
//
// Possible errors:
// - `REPROC_NOT_ENOUGH_MEMORY`
// - `REPROC_INVALID_UNICODE`
REPROC_ERROR string_to_wstring(const char *string, wchar_t **result);

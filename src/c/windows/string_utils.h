#pragma once

#include "process.h"

#include <wchar.h>

/* Joins all the strings in string_array together delimited by spaces */
PROCESS_LIB_ERROR string_join(const char **string_array, int array_length,
                              char **result);

PROCESS_LIB_ERROR string_to_wstring(const char *string, wchar_t **result);

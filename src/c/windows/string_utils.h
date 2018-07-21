#ifndef REPROC_C_WINDOWS_STRING_UTILS_H
#define REPROC_C_WINDOWS_STRING_UTILS_H

#include "reproc/error.h"

#include <wchar.h>

/* Joins all the strings in string_array together delimited by spaces */
REPROC_ERROR string_join(const char *const *string_array, int array_length,
                         char **result);

REPROC_ERROR string_to_wstring(const char *string, wchar_t **result);

REPROC_ERROR wstring_to_string(const wchar_t *wstring, char **result);

#endif

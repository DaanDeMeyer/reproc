#pragma once

#include "process.h"

#include <stdint.h>
#include <windows.h>

/* Create pipe and make sure the handle indicated by do_not_inherit is not
 * inherited.
 */
PROCESS_LIB_ERROR pipe_init(HANDLE *read, HANDLE *write);

PROCESS_LIB_ERROR pipe_disable_inherit(HANDLE pipe);

PROCESS_LIB_ERROR pipe_read(HANDLE pipe, void *buffer, uint32_t to_read,
                        uint32_t *actual);

PROCESS_LIB_ERROR pipe_write(HANDLE pipe, const void *buffer, uint32_t to_write,
                         uint32_t *actual);

/* Joins all the strings in string_array together using a single whitespace as
 * the delimiter.
 */
char *string_join(char **string_array, int array_length);

/* Converts narrow string (uft-8) to wide string (utf-16) */
wchar_t *string_to_wstring(const char *string);

/* Returns a matching process error for the given system error. Returns
 * PROCESS_UNKNOWN_ERROR if no matching process error is defined for the given
 * system error.
 */
PROCESS_LIB_ERROR system_error_to_process_error(DWORD system_error);
#pragma once

#include "process.h"

#include <stdint.h>
#include <windows.h>

PROCESS_LIB_ERROR pipe_init(HANDLE *read, HANDLE *write);

PROCESS_LIB_ERROR pipe_disable_inherit(HANDLE pipe);

PROCESS_LIB_ERROR pipe_write(HANDLE pipe, const void *buffer, uint32_t to_write,
                             uint32_t *actual);

PROCESS_LIB_ERROR pipe_read(HANDLE pipe, void *buffer, uint32_t to_read,
                            uint32_t *actual);

PROCESS_LIB_ERROR handle_close(HANDLE *handle_address);

/* Joins all the strings in string_array together delimited by spaces */
PROCESS_LIB_ERROR string_join(const char **string_array, int array_length,
                              char **result);

PROCESS_LIB_ERROR string_to_wstring(const char *string, wchar_t **result);

#pragma once

#include "process.h"

#include <stdint.h>
#include <windows.h>

/* Create pipe and make sure the handle indicated by do_not_inherit is not
 * inherited.
 */
PROCESS_LIB_ERROR pipe_init(HANDLE *read, HANDLE *write);

PROCESS_LIB_ERROR pipe_disable_inherit(HANDLE pipe);

PROCESS_LIB_ERROR pipe_write(HANDLE pipe, const void *buffer, uint32_t to_write,
                             uint32_t *actual);

PROCESS_LIB_ERROR pipe_write_fully(HANDLE pipe, const void *buffer,
                                   uint32_t to_write, uint32_t *actual);

PROCESS_LIB_ERROR pipe_read(HANDLE pipe, void *buffer, uint32_t to_read,
                            uint32_t *actual);

PROCESS_LIB_ERROR pipe_read_fully(HANDLE pipe, const void *buffer,
                                  uint32_t to_read, uint32_t *actual);

PROCESS_LIB_ERROR handle_close(HANDLE *handle);

/* Joins all the strings in string_array together using a single whitespace as
 * the delimiter.
 */
PROCESS_LIB_ERROR string_join(const char **string_array, int array_length,
                              char **result);

/* Converts narrow string (uft-8) to wide string (utf-16) */
PROCESS_LIB_ERROR string_to_wstring(const char *string, wchar_t **result);

#pragma once

#include "process.h"

#include <stdint.h>

/* Creates a pipe with pipe() and if the call succeeds assign the read endpoint
 * of the pipe to read and the write endpoint to write.
 */
PROCESS_LIB_ERROR pipe_init(int *read, int *write);

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer, uint32_t to_write,
                             uint32_t *actual);

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, uint32_t to_read,
                            uint32_t *actual);

/* Returns a matching process error for the given system error. Returns
 * PROCESS_UNKNOWN_ERROR if no matching process error is defined for the given
 * system error.
 */
PROCESS_LIB_ERROR system_error_to_process_error(int system_error);

#pragma once

#include "process.h"

/* Creates a pipe with pipe() and if the call succeeds assign the read endpoint
 * of the pipe to read and the write endpoint to write.
 */
PROCESS_ERROR pipe_init(int *read, int *write);

/* Returns a matching process error for the given system error. Returns
 * PROCESS_UNKNOWN_ERROR if no matching process error is defined for the given
 * system error.
 */
PROCESS_ERROR system_error_to_process_error(int system_error);
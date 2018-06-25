#pragma once

#include <stdint.h>

static const uint32_t PROCESS_LIB_INFINITE = 0xFFFFFFFF;

/* Opaque pointer to struct process *. The typedef is uppercased since _t suffix
 * is reserved by POSIX.
 */
typedef struct process Process;

typedef enum {
  PROCESS_LIB_SUCCESS = 0,
  PROCESS_LIB_UNKNOWN_ERROR = -1,
  PROCESS_LIB_WAIT_TIMEOUT = -2,
  PROCESS_LIB_STREAM_CLOSED = -3,
  PROCESS_LIB_STILL_RUNNING = -4,
  PROCESS_LIB_MEMORY_ERROR = -5,
  PROCESS_LIB_PIPE_LIMIT_REACHED = -6,
  PROCESS_LIB_INTERRUPTED = -7,
  PROCESS_LIB_IO_ERROR = -8,
  PROCESS_LIB_PROCESS_LIMIT_REACHED = -9,
  PROCESS_LIB_INVALID_UNICODE = -10
} PROCESS_LIB_ERROR;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocates memory for the Process pointer and initializes it.
 *
 * Aside from allocating the required memory for the Process pointer this
 * function also allocates the pipes used to redirect the standard streams of
 * the child process (stdin/stdout/stderr).
 *
 * Possible Errors:
 *   - PROCESS_LIB_MEMORY_ERROR: When allocating the Process pointer fails
 *   - PROCESS_LIB_PIPE_LIMIT_REACHED: When one or more of the pipes could not
 *     be allocated
 *
 * @param process_address Address of a Process pointer. Cannot be NULL. Any
 * already allocated memory to the Process pointer will be discarded.
 * @return PROCESS_LIB_ERROR
 */
PROCESS_LIB_ERROR process_init(Process **process_address);

/* argc and argv follow the conventions of the main function in c/c++ programs.
 * argv must begin with the name of the program to execute and end with a NULL
 * value (example: ["ls", "-l", NULL]). argc represents the number of arguments
 * excluding the NULL value (for the previous example, argc would be 2).
 */
PROCESS_LIB_ERROR process_start(Process *process, int argc, const char *argv[],
                                const char *working_directory);

PROCESS_LIB_ERROR process_write(Process *process, const void *buffer,
                                uint32_t to_write, uint32_t *actual);

PROCESS_LIB_ERROR process_read(Process *process, void *buffer, uint32_t to_read,
                               uint32_t *actual);

PROCESS_LIB_ERROR process_read_stderr(Process *process, void *buffer,
                                      uint32_t to_read, uint32_t *actual);

/* Waits the specified amount of time for the process to exit. if the timeout is
 * exceeded PROCESS_WAIT_TIMEOUT is returned. If milliseconds is
 * PROCESS_LIB_INFINITE the function waits indefinitely for the process to exit.
 */
PROCESS_LIB_ERROR process_wait(Process *process, uint32_t milliseconds);

/* Tries to terminate the process cleanly (allows for cleanup of resources). On
 * Windows a CTRL-BREAK signal is sent to the process. On POSIX a SIGTERM signal
 * is sent to the process. The function waits for the specified amount of
 * milliseconds for the process to exit. if the timeout is exceeded
 * PROCESS_WAIT_TIMEOUT is returned. If milliseconds is PROCESS_LIB_INFINITE the
 * function waits indefinitely for the process to exit.
 */
PROCESS_LIB_ERROR process_terminate(Process *process, uint32_t milliseconds);

/* Kills the process without allowing for cleanup. On Windows TerminateProcess
 * is called. On POSIX a SIGKILL signal is sent to the process. if the timeout
 * is exceeded PROCESS_WAIT_TIMEOUT is returned. If milliseconds is
 * PROCESS_LIB_INFINITE the function waits indefinitely for the process to exit.
 */
PROCESS_LIB_ERROR process_kill(Process *process, uint32_t milliseconds);

PROCESS_LIB_ERROR process_exit_status(Process *process, int32_t *exit_status);

/* Releases all resources associated with the process. Call this function if no
 * further interaction with the process is necessary. Call process_terminate or
 * process_kill first if you want to stop the process
 */
PROCESS_LIB_ERROR process_free(Process **process_address);

/* Returns the last system error code. On Windows the result of GetLastError
 * is returned and on POSIX the value of errno is returned. The value is not
 * stored so other functions that modify the results of GetLastError or
 * errno should not be called if you want to retrieve the last system error
 * that occurred in one of process-lib's functions.
 */
int64_t process_system_error(void);

#ifdef __cplusplus
}
#endif

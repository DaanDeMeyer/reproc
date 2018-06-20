#pragma once

#include <stdint.h>

#define INFINITE 0xFFFFFFFF

typedef struct process process;

typedef enum {
  PROCESS_SUCCESS = 0,
  PROCESS_UNKNOWN_ERROR = -1,
  PROCESS_WAIT_TIMEOUT = -2,
  PROCESS_STREAM_CLOSED = -3,
  PROCESS_CLOSE_ERROR = -4
} PROCESS_ERROR;

/* argc and argv follow the conventions of the main function in c/c++ programs.
 * argv must begin with the name of the program to execute and end with a NULL
 * value (example: ["ls", "-l", NULL]). argc represents the number of arguments
 * excluding the NULL value (for the previous example, argc would be 2).
 */
PROCESS_ERROR process_init(process *process);

PROCESS_ERROR process_start(process *process, int argc, char *argv[]);

PROCESS_ERROR process_write_stdin(process *process, const void *buffer,
                                  uint32_t to_write, uint32_t *actual);

PROCESS_ERROR process_read_stdout(process *process, void *buffer,
                                  uint32_t to_read, uint32_t *actual);

PROCESS_ERROR process_read_stderr(process *process, void *buffer,
                                  uint32_t to_read, uint32_t *actual);

/* Waits the specified amount of time for the process to exit. if the timeout is
 * exceeded PROCESS_WAIT_TIMEOUT is returned. If milliseconds is INFINITE the
 * function waits indefinitely for the process to exit.
 */
PROCESS_ERROR process_wait(process *process, uint32_t milliseconds);

/* Tries to terminate the process cleanly (allows for cleanup of resources). On
 * Windows a CTRL-BREAK signal is sent to the process. On POSIX a SIGTERM signal
 * is sent to the process. The function waits for the specified amount of
 * milliseconds for the process to exit. if the timeout is exceeded
 * PROCESS_WAIT_TIMEOUT is returned. If milliseconds is INFINITE the function
 * waits indefinitely for the process to exit.
 */
PROCESS_ERROR process_terminate(process *process, uint32_t milliseconds);

/* Kills the process without allowing for cleanup. On Windows TerminateProcess
 * is called. On POSIX a SIGKILL signal is sent to the process. if the timeout
 * is exceeded PROCESS_WAIT_TIMEOUT is returned. If milliseconds is INFINITE the
 * function waits indefinitely for the process to exit.
 */
PROCESS_ERROR process_kill(process *process, uint32_t milliseconds);

/* Releases all resources associated with the process. Call this function if no
 * further interaction with the process is necessary. Call process_terminate or
 * process_kill first if you want to stop the process
 */
PROCESS_ERROR process_free(process *process);

/* Returns the last system error code. On Windows the result of GetLastError
 * is returned and on POSIX the value of errno is returned. The value is not
 * stored so other functions that modify the results of GetLastError or
 * errno should not be called if you want to retrieve the last system error
 * that occurred in one of process-lib's functions.
 */
int64_t process_system_error(void);

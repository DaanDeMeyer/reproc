/*! \file process.h */

#pragma once

/*! Used to indicate that a function that takes a timeout value should wait
indefinitely. */
static const unsigned int PROCESS_LIB_INFINITE = 0xFFFFFFFF;

/*! Used to store child process information between multiple library calls */
typedef struct process process_type;

typedef enum {
  /*!
  Indicates a library call was successful. Because it evaluates to zero it can
  be used as follows:

  \code{.c}
  PROCESS_LIB_ERROR error = process_read(...);
  if (error) { return error; } // Only executes if library call is not a success
  \endcode
  */
  PROCESS_LIB_SUCCESS = 0,
  /*! Unlike POSIX, Windows does not include information about exactly which
  errors can occur in its documentation. If we don't know which error occurs we
  return PROCESS_LIB_UNKNOWN_ERROR when an error occurs on Windows. All
  functions can return this error. */
  PROCESS_LIB_UNKNOWN_ERROR = -1,
  /*! Returned if a timeout value passed to a function expired. */
  PROCESS_LIB_WAIT_TIMEOUT = -2,
  /*! Returned when the child process closes one of its streams (and in case of
  stdout/stderr all of the data from that stream has been read. */
  PROCESS_LIB_STREAM_CLOSED = -3,
  /*! Returned if \see process_exit_status is called while the child process is
  still running. */
  PROCESS_LIB_STILL_RUNNING = -4,
  /*! Returned if a memory allocation in the library code fails (Windows only)
  or the underlying system does not have enough memory to execute a system call.
  */
  PROCESS_LIB_MEMORY_ERROR = -5,
  /*! Returned if the current process is not allowed to create any more pipes.
   */
  PROCESS_LIB_PIPE_LIMIT_REACHED = -6,
  /*! Returned if a waiting system call (read, write, close, ...) was
  interrupted by the system. */
  PROCESS_LIB_INTERRUPTED = -7,
  /*! (POSIX) Returned when something goes wrong during I/O. The Linux docs do
  not go in depth on when exactly this error occurs. */
  PROCESS_LIB_IO_ERROR = -8,
  /*! Returned if the current process is not allowed to spawn any more child
  processes. */
  PROCESS_LIB_PROCESS_LIMIT_REACHED = -9,
  /*! (Windows) Returned if any of the UTF-8 strings passed to the library do
  not contain valid unicode. */
  PROCESS_LIB_INVALID_UNICODE = -10
} PROCESS_LIB_ERROR;

#ifdef __cplusplus
extern "C" {
#endif

/*!
Returns the size of process_type on the current platform so it can be allocated.
*/
unsigned int process_size();

/*!
Initializes the members of process_type.

Every call to process_init should be followed with a call to \see process_free
after the child process has exited.

\param[out] process_address Address of a process pointer. Cannot be NULL. Any
already allocated memory to the process pointer will be leaked.

\return PROCESS_LIB_ERROR
*/
PROCESS_LIB_ERROR process_init(process_type *process);

/*!
Starts the process specified by argv in the given working directory and
redirects its standard output streams.

This function should be called after process_init. After this function completes
(without error) the child process actually starts running and can be seen in the
Task Manager (Windows) or top (Linux).

\param[in,out] process Cannot be NULL.

\param[in] argv An array of UTF-8 encoded, null terminated strings specifying
the program to execute along with its arguments.

- The first element in the array indicates the program to run. This can be an
absolute path, a path relative to the working_directory (also passed as
argument) or the name of an executable located in the PATH. Cannot be NULL.
- The following elements indicate the whitespace delimited arguments passed to
the executable. None of these elements can be NULL.
- The final element must be NULL.

Example: ["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", NULL]

\param[in] argc Specifies the amount of elements in argv. Should NOT include the
NULL value at the end of the array.

Example: if argv is ["cmake", "--help", NULL] then argc is 2.

\param[in] working_directory Specified the working directory for the child
process. If working_directory is NULL, the child process runs in the same
directory as the current process.

\return PROCESS_LIB_ERROR

Possible errors:
- PROCESS_LIB_PIPE_LIMIT_REACHED
- PROCESS_LIB_PROCESS_LIMIT_REACHED
- PROCESS_LIB_MEMORY_ERROR
- PROCESS_LIB_INTERRUPTED
- PROCESS_LIB_IO_ERROR
- PROCESS_LIB_INVALID_UNICODE
*/
PROCESS_LIB_ERROR process_start(process_type *process, const char *argv[],
                                int argc, const char *working_directory);

/*!
Writes up to \p to_write bytes from \p buffer to the standard input (stdin) of
the child process and stores the amount of bytes written in \p actual.

For pipes, the write system call on both Windows and POSIX platforms will block
until the requested amount of bytes have been written to the pipe so this
function should only rarely succeed without writing the full amount of bytes
requested.

\param[in,out] process Cannot be NULL.
\param[in] buffer Pointer to memory block from which bytes should be written.
\param[in] to_write Maximum amount of bytes to write.
\param[out] actual Actual amount of bytes written. Set to 0 if an error occurs.

\return PROCESS_LIB_ERROR

Possible errors:
- PROCESS_LIB_STREAM_CLOSED
- PROCESS_LIB_INTERRUPTED
- PROCESS_LIB_IO_ERROR
*/
PROCESS_LIB_ERROR process_write(process_type *process, const void *buffer,
                                unsigned int to_write, unsigned int *actual);

/*!
Reads up to \p to_read bytes from the child process' standard output and
stores them in \p buffer. \p actual is set to the amount of bytes actually read.

Assuming no other errors occur this function keeps returning PROCESS_LIB_SUCCESS
until the child process closes its standard output stream (happens automatically
when it exits) and all bytes have been read from the stream. This allows the
function to be used in the following way to read all data from a child process'
stdout:

\code{.cpp}
std::stringstream ss;

while (true) {
  error = process_read(process, buffer, buffer_size - 1, &actual);
  if (error) { break; }

  buffer[actual] = '\0';
  ss << buffer;
}
\endcode

Remember that this function reads bytes and not strings. It is up to the user to
put a null terminator if he wants to use \p buffer as a string after this
function completes.

\param[in,out] process Cannot be NULL.
\param[out] buffer Pointer to memory block where bytes read from stdout should
be stored.
\param[in] to_read Maximum number of bytes to read from stdout.
\param[out] actual Actual amount of bytes read from stdout.

\return PROCESS_LIB_ERROR

Possible errors:
- PROCESS_LIB_STREAM_CLOSED
- PROCESS_LIB_INTERRUPTED
- PROCESS_LIB_IO_ERROR
*/
PROCESS_LIB_ERROR process_read(process_type *process, void *buffer,
                               unsigned int to_read, unsigned int *actual);

/*! \see process_read for the standard error stream of the child process.
 */
PROCESS_LIB_ERROR process_read_stderr(process_type *process, void *buffer,
                                      unsigned int to_read,
                                      unsigned int *actual);

/*!
Waits the specified amount of time for the process to exit.

\param[in,out] process Cannot be NULL.
\param[in] milliseconds Maximum amount of milliseconds to wait. If 0 the
function will only check if the process is still running without waiting. If
PROCESS_LIB_INFINITE the function will wait indefinitely for the child process
to exit.

\return PROCESS_LIB_ERROR

Possible errors when milliseconds is 0 or PROCESS_LIB_INFINITE:
- PROCESS_LIB_WAIT_TIMEOUT
- PROCESS_LIB_INTERRUPTED

Additional errors when milliseconds is not 0 or PROCESS_LIB_INFINITE:
- (POSIX) PROCESS_LIB_PROCESS_LIMIT_REACHED
- (POSIX) PROCESS_LIB_MEMORY_ERROR
*/
PROCESS_LIB_ERROR process_wait(process_type *process,
                               unsigned int milliseconds);

/*!
Tries to terminate the child process cleanly (the child process has a chance to
clean up).

On Windows a CTRL-BREAK signal is sent to the child process. On POSIX a SIGTERM
signal is sent to the child process. After sending the signal the function waits
for the specified amount of milliseconds for the child process to exit. If the
process has already exited no signal is sent.

\param[in,out] process Cannot be NULL.
\param[in] milliseconds See \see process_wait

\return PROCESS_LIB_ERROR

Possible errors: See \see process_wait
*/
PROCESS_LIB_ERROR process_terminate(process_type *process,
                                    unsigned int milliseconds);

/*!
Kills the child process without allowing for cleanup.

On Windows TerminateProcess is called. On POSIX a SIGKILL signal is sent to the
child process. if the timeout is exceeded PROCESS_WAIT_TIMEOUT is returned.
After sending the signal the function waits for the specified amount of
milliseconds for the child process to exit. If the child process has already
exited no signal is sent.

\param[in,out] process Cannot be NULL.
\param[in] milliseconds See \see process_wait

\return PROCESS_LIB_ERROR

Possible errors: See \see process_wait
*/
PROCESS_LIB_ERROR process_kill(process_type *process,
                               unsigned int milliseconds);

PROCESS_LIB_ERROR process_exit_status(process_type *process, int *exit_status);

/*!
Releases all resources associated with the process.

This function does not stop the child process. Call process_terminate or
process_kill first if you want to stop the child process.

All resources will be freed regardless if any error occurs or not. This function
should not be called again if an error occurs.

\param[in,out] process_address Address of a process_type pointer. Cannot be NULL
and has to point to a process_Type pointer initialized with \see process_start.
The process pointer is set to NULL after it is freed.

\return PROCESS_LIB_ERROR

Possible errors:
- PROCESS_LIB_INTERRUPTED
- PROCESS_LIB_IO_ERROR
*/
PROCESS_LIB_ERROR process_free(process_type *process);

/*!
Returns the last system error code.

On Windows this function returns the result of GetLastError. On POSIX this
function returns the value of errno. The value is not stored so other functions
that modify the results of GetLastError or errno should not be called if you
want to retrieve the last system error that occurred in one of process-lib's
functions.
*/
unsigned int process_system_error(void);

#ifdef __cplusplus
}
#endif

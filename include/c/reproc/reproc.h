/*! \file reproc.h */

#ifndef REPROC_H
#define REPROC_H

#if defined(_WIN32) && defined(REPROC_SHARED)
#if defined(REPROC_BUILDING)
#define REPROC_EXPORT __declspec(dllexport)
#else
#define REPROC_EXPORT __declspec(dllimport)
#endif
#else
#define REPROC_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
struct reproc {
  // unsigned long = DWORD
  unsigned long id;
  // void * = HANDLE
  void *handle;
  void *parent_stdin;
  void *parent_stdout;
  void *parent_stderr;
};
#else
struct reproc {
  int id;
  int parent_stdin;
  int parent_stdout;
  int parent_stderr;
  int exit_status;
};
#endif

/*! Used to store child process information between multiple library calls */
typedef struct reproc reproc_type;

/*! Used to indicate that a function that takes a timeout value should wait
indefinitely. */
extern REPROC_EXPORT const unsigned int REPROC_INFINITE;

/* When editing make sure to change the corresponding enum in reproc.hpp as
well */
typedef enum {
  /*!
  Indicates a library call was successful. Because it evaluates to zero it can
  be used as follows:

  \code{.c}
  REPROC_ERROR error = reproc_read(...);
  if (error) { return error; } // Only executes if library call is not a
  success \endcode

  All library function return this value if no error occured.
  */
  REPROC_SUCCESS,
  /*! Unlike POSIX, Windows does not include information about exactly which
  errors can occur in its documentation. If an error occurs that is not known
  functions will return REPROC_UNKNOWN_ERROR. */
  REPROC_UNKNOWN_ERROR,
  /*! A timeout value passed to a function expired. */
  REPROC_WAIT_TIMEOUT,
  /*! The child process closed one of its streams (and in case of
  stdout/stderr all of the data from that stream has been read). */
  REPROC_STREAM_CLOSED,
  /*! \see reproc_exit_status was called while the child process was still
  running. */
  REPROC_STILL_RUNNING,
  /*! A memory allocation in the library code failed (Windows only) or the
  underlying system did not have enough memory to execute a system call. */
  REPROC_MEMORY_ERROR,
  /*! The current or child process was not allowed to create any more pipes.
   */
  REPROC_PIPE_LIMIT_REACHED,
  /*! A waiting system call (read, write, close, ...) was interrupted by the
  system. */
  REPROC_INTERRUPTED,
  /*! The current process was not allowed to spawn any more child processes.
   */
  REPROC_PROCESS_LIMIT_REACHED,
  /*! One of the UTF-8 strings passed to the library did not contain
  valid unicode. */
  REPROC_INVALID_UNICODE,
  /*! The current process does not have permission to execute the program */
  REPROC_PERMISSION_DENIED,
  /*! Too many symlinks were encountered while looking for the program */
  REPROC_SYMLINK_LOOP,
  /*! The program or working directory was not found */
  REPROC_FILE_NOT_FOUND,
  /*! The given name was too long (most system have path length limits) */
  REPROC_NAME_TOO_LONG,
  /*! Only part of the buffer was written to the stdin of the child prcess */
  REPROC_PARTIAL_WRITE
} REPROC_ERROR;

/*!
Initializes the members of \p reproc.

Call order:
- \see reproc_init
- \see reproc_start
- ... (read/write/terminate/...)
- \see reproc_destroy
- \see reproc_init (If you want to reuse \p reproc to run another process)
- ...

\param[in,out] reproc Cannot be NULL.

\return REPROC_ERROR

Possible errors:
*/
REPROC_EXPORT REPROC_ERROR reproc_init(reproc_type *reproc);

/*!
Starts the process specified by argv in the given working directory and
redirects its standard output streams.

This function should be called after \see reproc_init. If this function
returns without error the child process actually starts running and can be
seen in the Task Manager (Windows) or top (Linux).

Every successful call to this function should be followed by a call to \see
reproc_destroy after the process has exited. If an error occurs the function
cleans up all allocated resources itself so you should call reproc_init again
if you want to retry.

\param[in,out] reproc Cannot be NULL. Must have been initialized with \see
reproc_init.

\param[in] argv An array of UTF-8 encoded, null terminated strings specifying
the program to execute along with its arguments. Must at least contain one
element. Cannot be NULL.

- The first element in the array indicates the executable to run. This can be
an absolute path, a path relative to the working directory (also passed as
argument) or the name of an executable located in the PATH. Cannot be NULL.
- The following elements indicate the whitespace delimited arguments passed to
the executable. None of these elements can be NULL.
- The final element must be NULL.

Example: ["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", NULL]

\param[in] argc Specifies the amount of elements in argv. Should NOT include
the NULL value at the end of the array. Must be bigger than or equal to 1.

Example: if argv is ["cmake", "--help", NULL] then argc is 2.

\param[in] working_directory Indicates the working directory for the child
process. If it is NULL, the child process runs in the same directory as the
current process.

\return REPROC_ERROR

Possible errors:
- REPROC_PIPE_LIMIT_REACHED
- REPROC_PROCESS_LIMIT_REACHED
- REPROC_MEMORY_ERROR
- REPROC_INVALID_UNICODE
- REPROC_PERMISSION_DENIED
- REPROC_SYMLINK_LOOP
- REPROC_FILE_NOT_FOUND
*/
REPROC_EXPORT REPROC_ERROR reproc_start(reproc_type *reproc, int argc,
                                        const char *const *argv,
                                        const char *working_directory);

/*!
Writes up to \p to_write bytes from \p buffer to the standard input (stdin) of
the child process and stores the amount of bytes written in \p bytes_written.

For pipes, the write system call on both Windows and POSIX platforms will
block until the requested amount of bytes have been written to the pipe so
this function should only rarely succeed without writing the full amount of
bytes requested.

\param[in,out] reproc Cannot be NULL.
\param[in] buffer Pointer to memory block from which bytes should be written.
\param[in] to_write Maximum amount of bytes to write.
\param[out] bytes_written Amount of bytes written. Set to zero if an error
occurs. Cannot be NULL.

\return REPROC_ERROR

Possible errors:
- REPROC_STREAM_CLOSED
- REPROC_INTERRUPTED
- REPROC_PARTIAL_WRITE
*/
REPROC_EXPORT REPROC_ERROR reproc_write(reproc_type *reproc, const void *buffer,
                                        unsigned int to_write,
                                        unsigned int *bytes_written);

/*!
Closes the standard input stream endpoint of the parent process.

This function is necessary when a child process reads from stdin until it is
closed. After writing all the input to the child process with \see
reproc_write the standard input stream can be closed using this function.

\param[in,out] reproc Cannot be NULL.

\return REPROC_ERROR

Possible errors:
- REPROC_INTERRUPTED
*/
REPROC_EXPORT REPROC_ERROR reproc_close_stdin(reproc_type *reproc);

/*!
Reads up to \p size bytes from the child process' standard output and stores
them in \p buffer. \p bytes_read is set to the amount of bytes read.

Assuming no other errors occur this function keeps returning REPROC_SUCCESS
until the child process closes its standard output stream (happens
automatically when it exits) and all bytes have been read from the stream.
This allows the function to be used in the following way to read all data from
a child process' stdout:

\code{.cpp}
std::stringstream ss;

while (true) {
  error = reproc_read(reproc, buffer, buffer_size - 1, &bytes_read);
  if (error) { break; }

  buffer[bytes_read] = '\0';
  ss << buffer;
}
\endcode

Remember that this function reads bytes and not strings. It is up to the user
to put a null terminator if he wants to use \p buffer as a string after this
function completes.

\param[in,out] reproc Cannot be NULL.
\param[out] buffer Pointer to memory block where bytes read from stdout should
be stored.
\param[in] size Maximum number of bytes to read from stdout.
\param[out] bytes_read Amount of bytes read from stdout. Set to zero if an
error occurs. Cannot be NULL.

\return REPROC_ERROR

Possible errors:
- REPROC_STREAM_CLOSED
- REPROC_INTERRUPTED
*/
REPROC_EXPORT REPROC_ERROR reproc_read(reproc_type *reproc, void *buffer,
                                       unsigned int size,
                                       unsigned int *bytes_read);

/*! \see reproc_read for the standard error stream of the child process. */
REPROC_EXPORT REPROC_ERROR reproc_read_stderr(reproc_type *reproc, void *buffer,
                                              unsigned int size,
                                              unsigned int *bytes_read);

/*!
Waits the specified amount of time for the process to exit.

\param[in,out] reproc Cannot be NULL.
\param[in] milliseconds Amount of milliseconds to wait. If it is 0 the
function will only check if the process is still running without waiting. If
it is REPROC_INFINITE the function will wait indefinitely for the child
process to exit.

\return REPROC_ERROR

Possible errors when \p milliseconds is REPROC_INFINITE:
- REPROC_INTERRUPTED

Possible errors when milliseconds is 0:
- REPROC_WAIT_TIMEOUT

Possible errors when milliseconds is not 0 or REPROC_INFINITE:
- REPROC_INTERRUPTED
- REPROC_PROCESS_LIMIT_REACHED
- REPROC_MEMORY_ERROR
*/
REPROC_EXPORT REPROC_ERROR reproc_wait(reproc_type *reproc,
                                       unsigned int milliseconds);

/*!
Tries to terminate the child process cleanly (the child process has a chance
to clean up).

On Windows a CTRL-BREAK signal is sent to the child process. On POSIX a
SIGTERM signal is sent to the child process. After sending the signal the
function waits for the specified amount of milliseconds for the child process
to exit. If the child process has already exited no signal is sent.

\param[in,out] reproc Cannot be NULL.
\param[in] milliseconds See \see reproc_wait.

\return REPROC_ERROR

Possible errors: See \see reproc_wait
*/
REPROC_EXPORT REPROC_ERROR reproc_terminate(reproc_type *reproc,
                                            unsigned int milliseconds);

/*!
Kills the child process without allowing for cleanup.

On Windows TerminateProcess is called. On POSIX a SIGKILL signal is sent to
the child process. if the timeout is exceeded REPROC_WAIT_TIMEOUT is returned.
After sending the signal the function waits for the specified amount of
milliseconds for the child process to exit. If the child process has already
exited no signal is sent.

This function should only be used as a last resort. Always try to stop a child
process with \see reproc_terminate before resorting to this function.

\param[in,out] reproc Cannot be NULL.
\param[in] milliseconds See \see reproc_wait.

\return REPROC_ERROR

Possible errors: See \see reproc_wait
*/
REPROC_EXPORT REPROC_ERROR reproc_kill(reproc_type *reproc,
                                       unsigned int milliseconds);

/*!
Returns the exit status of the child process

\param[in,out] reproc Cannot be NULL.
\param[out] exit_status Exit status of the process. Cannot be NULL.

\return REPROC_ERROR

Possible errors:
- REPROC_STILL_RUNNING
*/
REPROC_EXPORT REPROC_ERROR reproc_exit_status(reproc_type *reproc,
                                              int *exit_status);

/*!
Releases all resources associated with the child process.

This function does not stop the child process. Call \see reproc_terminate or
\see reproc_kill first if you want to stop the child process or wait for it to
exit on its own with \see reproc_wait.

\param[in,out] reproc Cannot be NULL

\return REPROC_ERROR

Possible errors:
*/
REPROC_EXPORT REPROC_ERROR reproc_destroy(reproc_type *reproc);

/*!
Returns the last system error code.

On Windows this function returns the result of GetLastError. On POSIX this
function returns the value of errno. The value is not stored so other
functions that modify the results of GetLastError or errno should not be
called if you want to retrieve the last system error that occurred in one of
reproc's functions.

On POSIX, if an error occurs after fork but before exec it is communicated to
the parent process which sets its own errno value to the errno value of the
child process. This makes it possible to also retrieve errors that happen
after forking with this function (for example in chdir or execve).

\return unsigned int The last system error code
*/
REPROC_EXPORT unsigned int reproc_system_error(void);

REPROC_EXPORT const char *reproc_error_to_string(REPROC_ERROR error);

#ifdef __cplusplus
}
#endif

#endif

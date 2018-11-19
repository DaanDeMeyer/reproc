/*! \file reproc.h */

#ifndef REPROC_H
#define REPROC_H

#include "reproc/error.h"
#include "reproc/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! Used to store information about a child process. We define reproc_type in
the header file so it can be allocated on the stack but its internals are prone
to change and should **NOT** be depended on. */
#if defined(_WIN32)
struct reproc_type {
  /*! \privatesection */
  // unsigned long = DWORD
  unsigned long id;
  // void * = HANDLE
  void *handle;
  void *in;
  void *out;
  void *err;
};
#else
struct reproc_type {
  /*! \privatesection */
  int id;
  int in;
  int out;
  int err;
};
#endif

// We can't name the struct reproc because reproc++'s namespace is already named
// reproc. reproc_t can't be used either because the _t suffix is reserved by
// POSIX so we fall back to reproc_type instead.
typedef struct reproc_type reproc_type;

/*!
Stream identifiers used to specify which stream to work with.

\see reproc_read
\see reproc_close
*/
typedef enum {
  /*! stdin */
  REPROC_IN = 0,
  /*! stdout */
  REPROC_OUT = 1,
  /*! stderr */
  REPROC_ERR = 2
} REPROC_STREAM;

/*! \see reproc_stop */
typedef enum {
  /*! noop (no operation) */
  REPROC_NOOP = 0,
  /*! #reproc_wait */
  REPROC_WAIT = 1,
  /*! #reproc_terminate */
  REPROC_TERMINATE = 2,
  /*! #reproc_kill */
  REPROC_KILL = 3
} REPROC_CLEANUP;

/*! Tells a function that takes a timeout value to wait indefinitely. */
REPROC_EXPORT extern const unsigned int REPROC_INFINITE;

/*!
Starts the process specified by \p argv in the given working directory and
redirects its standard input and output streams.

If this function does not return an error the child process will have started
running and can be inspected using the operating system's tools for process
inspection (e.g. ps on Linux).

Every successful call to this function should be followed by a successful call
to #reproc_wait, #reproc_terminate or #reproc_kill and a call to
#reproc_destroy. If an error occurs during #reproc_start all allocated resources
are cleaned up before #reproc_start returns and no further action is required.

\param[in,out] process Cannot be `NULL`.
\param[in] argv
\parblock
An array of UTF-8 encoded, null terminated strings that specifies the program to
execute along with its arguments. Must contain at least one element. Cannot be
`NULL`.

- The first element in the array indicates the executable to run as a child
process. This can be an absolute path, a path relative to the working directory
(also passed as argument) or the name of an executable located in the PATH. It
cannot be `NULL`.
- The following elements indicate the whitespace delimited arguments passed to
the executable. None of these elements can be `NULL`.
- The final element must be `NULL`.

Example: ["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", `NULL`]
\endparblock
\param[in] argc
\parblock
Specifies the amount of elements in \p argv. Should NOT count the `NULL` value
at the end of the array. Must be bigger than or equal to 1.

Example: if argv is ["cmake", "--help", `NULL`] then \p argc is 2.
\endparblock
\param[in] working_directory Indicates the working directory for the child
process. If it is `NULL`, the child process runs in the same directory as the
parent process.

\return
\parblock
#REPROC_ERROR

Possible errors:
- #REPROC_PIPE_LIMIT_REACHED
- #REPROC_PROCESS_LIMIT_REACHED
- #REPROC_NOT_ENOUGH_MEMORY
- #REPROC_INVALID_UNICODE
- #REPROC_PERMISSION_DENIED
- #REPROC_SYMLINK_LOOP
- #REPROC_FILE_NOT_FOUND
\endparblock
*/
REPROC_EXPORT REPROC_ERROR reproc_start(reproc_type *process, int argc,
                                        const char *const *argv,
                                        const char *working_directory);

/*!
Reads up to \p size bytes from the child process' standard output and stores
them in \p buffer. \p bytes_read is used to store the amount of bytes read from
\p stream.

Assuming no other errors occur this function will return #REPROC_SUCCESS until
the child process closes the stream indicated by \p stream (the stream is closed
automatically when the child process exits) and all bytes have been read from
the given stream. This allows the function to be used in the following way to
read all data from a child process' stdout stream (the example uses C++
std::string for brevity):

\code{.cpp}
#define BUFFER_SIZE 1024

...

char buffer[BUFFER_SIZE];
std::string output{};

while (true) {
  unsigned int bytes_read = 0;
  error = reproc_read(process, REPROC_OUT, buffer, BUFFER_SIZE, &bytes_read);
  if (error) { break; }

  output.append(buffer, bytes_read);
}

if (error != REPROC_STREAM_CLOSED) { return error; }

// Do something with the output
\endcode

Remember that this function reads bytes and not strings. A null terminator has
to be added to \p buffer after reading to be able to use \p buffer as a null
terminated string. This is easily accomplished as long as we make sure to leave
space for the null terminator in the buffer when reading:

\code{.c}
unsigned int bytes_read = 0;
error = reproc_read(process, REPROC_OUT, buffer, BUFFER_SIZE - 1, &bytes_read);
//                                               ^^^^^^^^^^^^^^^
if (error) { return error; }

buffer[bytes_read] = '\0'; // Add null terminator
\endcode

\param[in,out] process Cannot be `NULL`.
\param[in] stream Stream to read from. Cannot be #REPROC_IN.
\param[out] buffer Buffer used to store bytes read from the given stream. Cannot
be `NULL`.
\param[in] size Size of \p buffer.
\param[out] bytes_read Amount of bytes read from the given stream. Set to zero
if an error occurs. Cannot be `NULL`.

\return
\parblock
#REPROC_ERROR

Possible errors:
- #REPROC_STREAM_CLOSED
- #REPROC_INTERRUPTED
\endparblock
*/
REPROC_EXPORT REPROC_ERROR reproc_read(reproc_type *process,
                                       REPROC_STREAM stream, void *buffer,
                                       unsigned int size,
                                       unsigned int *bytes_read);

/*!
Writes up to \p to_write bytes from \p buffer to the standard input (stdin) of
the child process and stores the amount of bytes written to \p stream in \p
bytes_written.

For pipes, the `write` system call on both Windows and POSIX platforms will
block until the requested amount of bytes have been written to the pipe so
this function should only rarely succeed without writing the full amount of
bytes requested.

(POSIX) By default, writing to a closed stdin pipe terminates the parent process
with the `SIGPIPE` signal. #reproc_write will only return #REPROC_STREAM_CLOSED
if this signal is ignored by the parent process.

\param[in,out] process Cannot be `NULL`.
\param[in] buffer Pointer to memory block from which bytes should be written.
Cannot be `NULL`.
\param[in] to_write Maximum amount of bytes to write to \p buffer.
\param[out] bytes_written Amount of bytes written to \p buffer. Set to
zero if an error occurs. Cannot be `NULL`.

\return
\parblock
#REPROC_ERROR

Possible errors:
- #REPROC_STREAM_CLOSED
- #REPROC_INTERRUPTED
- #REPROC_PARTIAL_WRITE
\endparblock
*/
REPROC_EXPORT REPROC_ERROR reproc_write(reproc_type *process,
                                        const void *buffer,
                                        unsigned int to_write,
                                        unsigned int *bytes_written);

/*!
Closes the stream endpoint of the parent process indicated by \p stream.

This function is necessary when a child process reads from stdin until it is
closed. After writing all the input to the child process with #reproc_write the
standard input stream can be closed using this function.

\param[in,out] process Cannot be NULL.
\param[in] stream The stream to close.

\return
\parblock
#REPROC_ERROR

Possible errors: /
\endparblock
*/
REPROC_EXPORT void reproc_close(reproc_type *process, REPROC_STREAM stream);

/*!
Waits \p timeout milliseconds for the child process to exit.

If the child process exits before the timeout expires the exit status is stored
in \p exit_status.

Should not be called after one of #reproc_wait, #reproc_terminate, #reproc_kill
or #reproc_stop has returned #REPROC_SUCCESS for that child process.

\param[in,out] process Cannot be `NULL`.
\param[in] timeout
\parblock
Amount of milliseconds to wait for the child process to exit.

If \p timeout is 0 the function will only check if the child process is still
running without waiting.

If \p timeout is #REPROC_INFINITE the function will wait indefinitely for the
child process to exit.
\endparblock
\param[out] Optional output parameter used to store the exit status of the child
process.

\return
\parblock
#REPROC_ERROR

Possible errors when \p timeout is 0 or #REPROC_INFINITE:
- #REPROC_INTERRUPTED
- #REPROC_WAIT_TIMEOUT

Extra possible errors when \p timeout is not 0 or #REPROC_INFINITE (POSIX only):
- #REPROC_PROCESS_LIMIT_REACHED
- #REPROC_NOT_ENOUGH_MEMORY
\endparblock
*/
REPROC_EXPORT REPROC_ERROR reproc_wait(reproc_type *process,
                                       unsigned int timeout,
                                       unsigned int *exit_status);

/*!
Sends the `SIGTERM` signal (POSIX) or the `CTRL-BREAK` signal (Windows) to the
child process and calls #reproc_wait with the given arguments.

Should not be called after one of #reproc_wait, #reproc_terminate, #reproc_kill
or #reproc_stop has returned #REPROC_SUCCESS for that child process.

\return Return value of #reproc_wait.

\see reproc_wait
*/
REPROC_EXPORT REPROC_ERROR reproc_terminate(reproc_type *process,
                                            unsigned int timeout,
                                            unsigned int *exit_status);

/*!
Sends the `SIGKILL` signal to the child process (POSIX) or calls
`TerminateProcess` (Windows) on the child process and calls #reproc_wait with
the given arguments.

If the child process exits before the timeout expires the exit status stored in
\p exit_status will be 137 (value of SIGKILL signal).

Should not be called after one of #reproc_wait, #reproc_terminate, #reproc_kill
or #reproc_stop has returned #REPROC_SUCCESS for that child process.

\return Return value of #reproc_wait.

\see reproc_wait
*/
REPROC_EXPORT REPROC_ERROR reproc_kill(reproc_type *process,
                                       unsigned int timeout,
                                       unsigned int *exit_status);

/*!
Simplifies calling combinations of #reproc_wait, #reproc_terminate and
#reproc_kill. The given steps are executed in the order they were given until
one of the steps succeeds or returns an error (except a timeout error).

Should not be called after one of #reproc_wait, #reproc_terminate, #reproc_kill
or #reproc_stop has returned #REPROC_SUCCESS for that child process.

Example:

Wait 10 seconds for the child process to exit on its own before sending
`SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waiting 5 more seconds for the
child process to exit.

\code{.c}
REPROC_ERROR error = reproc_stop(process,
                                 REPROC_WAIT, 10000,
                                 REPROC_TERMINATE, 5000,
                                 REPROC_NOOP, 0);
\endcode

Call #reproc_wait, #reproc_terminate and #reproc_kill directly if you need extra
logic such as logging between calls.

\param[in,out] process Cannot be `NULL`.
\param[in] c1,c2,c3 Instruct `reproc_stop` to execute the function associated
with the value in #REPROC_CLEANUP and the associated timeout.
\param[in] t1,t2,t3 Associated timeout values for \p c1, \p c2 and \p c3.
\param[out] Optional output parameter used to store the exit status of the child
process.

\return #REPROC_ERROR \see reproc_wait

*/
REPROC_EXPORT REPROC_ERROR reproc_stop(reproc_type *process, REPROC_CLEANUP c1,
                                       unsigned int t1, REPROC_CLEANUP c2,
                                       unsigned int t2, REPROC_CLEANUP c3,
                                       unsigned int t3,
                                       unsigned int *exit_status);

/*!
Frees all allocated resources stored in \p process. Should only be called after
the child process has exited.

\param[in,out] process Cannot be `NULL`.
*/
REPROC_EXPORT void reproc_destroy(reproc_type *process);

#ifdef __cplusplus
}
#endif

#endif

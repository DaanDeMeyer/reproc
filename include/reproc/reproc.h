/*! \file reproc.h */

#ifndef REPROC_H
#define REPROC_H

#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! Used to store information about a child process. The internals are public so
the struct can be allocated on the stack but are prone to change and should NOT
be used by users. */
#if defined(_WIN32)
struct reproc_type {
  /*! \privatesection */
  // unsigned long = DWORD
  unsigned long id;
  // void * = HANDLE
  void *handle;
  void *parent_stdin;
  void *parent_stdout;
  void *parent_stderr;
};
#else
struct reproc_type {
  /*! \privatesection */
  int id;
  int parent_stdin;
  int parent_stdout;
  int parent_stderr;
};
#endif

typedef struct reproc_type reproc_type; // _t is reserved by POSIX

/*!
Stream identifiers used to indicate which stream to close or read from.

\see reproc_read
\see reproc_close
*/
typedef enum {
  /*! stdin */
  REPROC_IN,
  /*! stdout */
  REPROC_OUT,
  /*! stderr */
  REPROC_ERR
} REPROC_STREAM;

/*!
Contains flags used to tell reproc how to stop a child process.

\see reproc_stop
*/
typedef enum {
  /*! Wait for the child process to exit on its own. */
  REPROC_WAIT = 1 << 0,
  /*! Send `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and wait for the child
  process to exit. */
  REPROC_TERMINATE = 1 << 1,
  /*! Send `SIGKILL` (POSIX) or call `TerminateProcess` (Windows) and wait for
  the child process to exit. Because `SIGKILL` and `TerminateProcess` do not
  allow the child process to clean up its resources this flag should only be
  used if the #REPROC_TERMINATE flag is not sufficient to stop a child process.
  */
  REPROC_KILL = 1 << 2
} REPROC_CLEANUP;

/*! Tells a function that takes a timeout value to wait indefinitely. */
static const unsigned int REPROC_INFINITE = 0xFFFFFFFF;

/*!
Starts the process specified by \p argv in the given working directory and
redirects its standard output streams.

If this function does not return an error the child process actually starts
executing and can be inspected using the operating system's tools for process
inspection (e.g. ps on Linux).

Every successful call to this function should be followed by a call to
#reproc_stop. If an error occurs all allocated resources are cleaned up before
the function returns.

\param[in,out] process Cannot be `NULL`.

\param[in] argv
\parblock
An array of UTF-8 encoded, null terminated strings specifying the program to
execute along with its arguments. Must at least contain one element. Cannot be
`NULL`.

- The first element in the array indicates the executable to run. This can be
an absolute path, a path relative to the working directory (also passed as
argument) or the name of an executable located in the PATH. Cannot be `NULL`.
- The following elements indicate the whitespace delimited arguments passed to
the executable. None of these elements can be `NULL`.
- The final element must be `NULL`.

Example: ["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", `NULL`]
\endparblock
\param[in] argc
\parblock
Specifies the amount of elements in \p argv. Should NOT include the `NULL` value
at the end of the array. Must be bigger than or equal to 1.

Example: if argv is ["cmake", "--help", `NULL`] then \p argc is 2.
\endparblock
\param[in] working_directory Indicates the working directory for the child.
process. If it is `NULL`, the child process runs in the same directory as the
current process.

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
Writes up to \p to_write bytes from \p buffer to the standard input (stdin) of
the child process and stores the amount of bytes written in \p bytes_written.

For pipes, the `write` system call on both Windows and POSIX platforms will
block until the requested amount of bytes have been written to the pipe so
this function should only rarely succeed without writing the full amount of
bytes requested.

(POSIX) Writing to a closed stdin pipe by default terminates the parent
process with the `SIGPIPE` signal. #reproc_write will only return
#REPROC_STREAM_CLOSED if this signal is ignored by the parent process.

\param[in,out] process Cannot be `NULL`.
\param[in] buffer Pointer to memory block from which bytes should be written.
\param[in] to_write Maximum amount of bytes to write.
\param[out] bytes_written Amount of bytes written. Set to zero if an error
occurs. Cannot be `NULL`.

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
Reads up to \p size bytes from the child process' standard output and stores
them in \p buffer. \p bytes_read is used to store the amount of bytes read.

Assuming no other errors occur this function returns #REPROC_SUCCESS until the
child process closes its standard output stream (happens automatically when the
child process exits) and all bytes have been read from the given stream. This
allows the function to be used in the following way to read all data from a
child process stdout stream (uses C++ std::string for brevity):

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

// Do something with output
\endcode

Remember that this function reads bytes and not strings. It is up to the user
to add a null terminator if he wants to use \p buffer as a null terminated
string after this function completes. However, this is easily accomplished as
long as we make sure to leave space for the null terminator in the buffer when
reading:

\code{.c}
unsigned int bytes_read = 0;
error = reproc_read(process, REPROC_OUT, buffer, BUFFER_SIZE - 1, &bytes_read);
//                                               ^^^^^^^^^^^^^^^
if (error) { return error; }

buffer[bytes_read] = '\0'; // Add null terminator
\endcode

\param[in,out] process Cannot be `NULL`.
\param[in] stream Stream to read from. Cannot be #REPROC_IN.
\param[out] buffer Buffer used to store bytes read from the given stream.
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
Tries to stop the child process according to the flags specified by \p
cleanup_flags and cleans up the resources allocated for the child process.

Note that the parent process has to observe that the child process has exited
before the remaining resources that are managed by the kernel are completely
cleaned up. Always make sure the combination of \p cleanup_flags and \p t1,t2,t3
stops the child process completely and allows the function to observe its exit
so that all kernel managed resources of the child process are correctly cleaned
up.

Regardless of whether this function observes that the child process has exited
or not, it cleans up all associated resources that are not managed by the
kernel. As a result, this function can only be called **once** per child
process.

\param[in,out] process Cannot be `NULL`.

\param[in] cleanup_flags Indicate which actions should be performed to stop
the process. This parameter takes a combination of flags from the
#REPROC_CLEANUP enum. Flags can be combined with a bitwise OR.

\param[in] t1,t2,t3
\parblock
Parameters containing a timeout value for each flag in \p cleanup_flags. Each
value specifies the amount of milliseconds to wait after the corresponding step
in stopping the process.

Values from \p t1,t2,t3 are assigned to enabled flags in the following order:

1. #REPROC_WAIT
2. #REPROC_TERMINATE
3. #REPROC_KILL

(If only one flag is specified only \p t1 is used, if two flags are specified \p
t1 and \p t2 are used, ...)

If a value is 0 the function will only check if the process is still running
without waiting after the corresponding step.

If a value is #REPROC_INFINITE the function will wait indefinitely after the
corresponding step for the child process to exit.
\endparblock

\param[out] exit_status
\parblock Optional output parameter used to store the exit status of the child
process.

If the child process exits normally the exit status of the child process is
stored in \p exit_status.

Exiting normally is defined as follows for the different platforms:
- POSIX: The process returned from its main function or called `exit` or
`_exit`.
- Windows: The process returned from its `main` or `WinMain` function.

If the process does not exit normally the following values are stored in \p
exit_status:
- POSIX: Number of the signal that terminated the child process.
- Windows: exit status passed to the `ExitProcess` or `TerminateProcess`
function call that terminated the child process. 
\endparblock

\return
\parblock
#REPROC_ERROR

Possible errors when \p t1,t2,t3 are all #REPROC_INFINITE:
- #REPROC_INTERRUPTED

Possible errors when \p t1,t2,t3 are all 0:
- #REPROC_WAIT_TIMEOUT

Possible errors when one of \p t1,t2,t3 is not 0 or #REPROC_INFINITE:
- #REPROC_INTERRUPTED
- #REPROC_WAIT_TIMEOUT
- #REPROC_PROCESS_LIMIT_REACHED
- #REPROC_NOT_ENOUGH_MEMORY
\endparblock

Example: if \p cleanup_flags is #REPROC_WAIT | #REPROC_KILL and \p t1,t2,t3
are `10000, 5000, 3000` the function will first wait for 10 seconds for the
child process to exit on its own. If the child process hasn't exited by then,
the function will send the `SIGTERM` signal (POSIX) or the `CTRL-BREAK` signal
(Windows) to the child process and wait 5 more seconds for the child process to
exit. If the child process is still running after waiting the
#REPROC_WAIT_TIMEOUT error is returned. Only two flags are specified so \p t3 is
ignored.
*/
REPROC_EXPORT REPROC_ERROR reproc_stop(reproc_type *process, int cleanup_flags,
                                       unsigned int t1, unsigned int t2,
                                       unsigned int t3,
                                       unsigned int *exit_status);

#ifdef __cplusplus
}
#endif

#endif

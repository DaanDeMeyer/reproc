/*! \file reproc.h */

#ifndef REPROC_H
#define REPROC_H

#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

// struct is defined in header file so it can be allocated on the stack
// Named reproc_type so we can use the reproc namespace in C++ API
#if defined(_WIN32)
struct reproc_type {
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
  int id;
  int parent_stdin;
  int parent_stdout;
  int parent_stderr;
};
#endif

/*! Used to store child process information between multiple library calls */
typedef struct reproc_type reproc_type; // _t is reserved by POSIX

typedef enum { REPROC_STDIN, REPROC_STDOUT, REPROC_STDERR } REPROC_STREAM;

/*! Used to indicate that a function that takes a timeout value should wait
indefinitely. */
REPROC_EXPORT extern const unsigned int REPROC_INFINITE;

/*!
Starts the process specified by argv in the given working directory and
redirects its standard output streams.

If this function does not return an error the child process actually starts
executing and can be inspected using the operating system's tools for process
inspection (e.g. ps on Linux).

Every successful call to this function should be followed by a call to \see
reproc_stop. If an error occurs all allocated resources are cleaned up before
the function returns.

\param[in,out] process Cannot be NULL.

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
- REPROC_NOT_ENOUGH_MEMORY
- REPROC_INVALID_UNICODE
- REPROC_PERMISSION_DENIED
- REPROC_SYMLINK_LOOP
- REPROC_FILE_NOT_FOUND
*/
REPROC_EXPORT REPROC_ERROR reproc_start(reproc_type *process, int argc,
                                        const char *const *argv,
                                        const char *working_directory);

/*!
Writes up to \p to_write bytes from \p buffer to the standard input (stdin) of
the child process and stores the amount of bytes written in \p bytes_written.

For pipes, the write system call on both Windows and POSIX platforms will
block until the requested amount of bytes have been written to the pipe so
this function should only rarely succeed without writing the full amount of
bytes requested.

(POSIX) Writing to a closed stdin pipe by default terminates the parent
process with the `SIGPIPE` signal. reproc_write will only return
REPROC_STREAM_CLOSED if this signal is ignored by the parent process.

\param[in,out] process Cannot be NULL.
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
REPROC_EXPORT REPROC_ERROR reproc_write(reproc_type *process,
                                        const void *buffer,
                                        unsigned int to_write,
                                        unsigned int *bytes_written);

/*!
Closes the stream endpoint of the parent process indicated by /p stream.

This function is necessary when a child process reads from stdin until it is
closed. After writing all the input to the child process with \see
reproc_write the standard input stream can be closed using this function.

\param[in,out] process Cannot be NULL.
\param[in] stream The stream to close.

\return REPROC_ERROR

Possible errors:
*/
REPROC_EXPORT void reproc_close(reproc_type *process, REPROC_STREAM stream);

/*!
Reads up to \p size bytes from the child process' standard output and stores
them in \p buffer. \p bytes_read is set to the amount of bytes read.

Assuming no other errors occur this function returns REPROC_SUCCESS until the
child process closes its standard output stream (happens automatically when it
exits) and all bytes have been read from the stream. This allows the function
to be used in the following way to read all data from a child process stdout
stream (uses C++ std::string for brevity):

\code{.cpp}
#define BUFFER_SIZE 1024

...

char buffer[BUFFER_SIZE];
std::string output{};

while (true) {
  unsigned int bytes_read = 0;
  error = reproc_read(process, REPROC_STDOUT, buffer, BUFFER_SIZE,
&bytes_read); if (error) { break; }

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
error = reproc_read(process, REPROC_STDOUT, buffer, BUFFER_SIZE - 1,
                    &bytes_read); //               ^^^^^^^^^^^^^^^
if (error) { return error; }

buffer[bytes_read] = '\0'; // Add null terminator
\endcode

\param[in,out] process Cannot be NULL.
\param[in] stream Stream to read from. Cannot be REPROC_STDIN.
\param[out] buffer Pointer to buffer where bytes read from stdout should be
stored.
\param[in] size Maximum number of bytes to read from stdout. \param[out]
bytes_read Amount of bytes read from stdout. Set to zero if an error occurs.
Cannot be NULL.

\return REPROC_ERROR

Possible errors:
- REPROC_STREAM_CLOSED
- REPROC_INTERRUPTED
*/
REPROC_EXPORT REPROC_ERROR reproc_read(reproc_type *process,
                                       REPROC_STREAM stream, void *buffer,
                                       unsigned int size,
                                       unsigned int *bytes_read);

typedef enum {
  /*! Wait for the child process to exit on its own */
  REPROC_WAIT = 1 << 0,
  /*! Send SIGTERM (POSIX) or CTRL-BREAK (Windows) and wait for the child
  process to exit */
  REPROC_TERMINATE = 1 << 1,
  /*! Send SIGKILL (POSIX) or call TerminateProcess (Windows) and wait for the
  child process to exit. Because SIGKILL and TerminateProcess do not allow the
  child process to clean up its resources this flag should only be used if the
  REPROC_TERMINATE flag is not sufficient to stop a child process. */
  REPROC_KILL = 1 << 2
} REPROC_CLEANUP;

/*!
Tries to stop the child process according to the flags specified by /p
cleanup_flags and cleans up all associated resources.

Regardless whether the child process is succesfully stopped or not this function
releases all resources associated with the child process. Because of this, it
can only be called once for each child process. Always make sure the combination
of \p cleanup_flags and \p timeout is enough to stop the child process
completely.

\param[in,out] process Cannot be NULL.

\param[in] cleanup_flags Indicate which actions should be performed to stop
the process. This parameter takes a combination of flags from the \see
REPROC_CLEANUP enum. Flags can be combined with a bitwise OR.

Example: if \p cleanup_flags is REPROC_WAIT | REPROC_TERMINATE the function will
first wait for \p timeout milliseconds for the child process to exit on its
own. If the child process hasn't exited after waiting, the function will then
send SIGTERM (POSIX) or CTRL-BREAK (Windows) to the child process and wait \p
timeout milliseconds again for the child process to exit. If the child
process is still running after waiting the REPROC_WAIT_TIMEOUT error is
returned.

\param[in] timeout Amount of milliseconds to wait after each step in
stopping the process.

If 0 the function will only check if the process is still running without
waiting after each step.

If REPROC_INFINITE the function will wait indefinitely after each step for the
child process to exit (this implies only one step will ever run even if multiple
steps are specified since the function will wait indefinitely after the first
step).

\param[out] exit_status Optional output parameter used to store the exit
status of the child process.

If the child process exits normally the exit status of the child process is
stored in \p exit_status.

Exiting normally is defined as follows for the different platforms:
- POSIX: The process returned from its main function or called exit or _exit
- Windows: The process returned from its main or WinMain function

If the function does not exit normally the following values are stored in \p
exit_status:
- POSIX: Number of the signal that terminated the child process
- Windows: exit status passed to ExitProcess or TerminateProcess function call
that terminated the child process

\return REPROC_ERROR

Possible errors when \p timeout is REPROC_INFINITE:
- REPROC_INTERRUPTED

Possible errors when \p timeout is 0:
- REPROC_WAIT_TIMEOUT

Possible errors when \p timeout is not 0 or REPROC_INFINITE:
- REPROC_INTERRUPTED
- REPROC_WAIT_TIMEOUT
- REPROC_PROCESS_LIMIT_REACHED
- REPROC_NOT_ENOUGH_MEMORY
*/
REPROC_EXPORT REPROC_ERROR reproc_stop(reproc_type *process, int cleanup_flags,
                                       unsigned int timeout,
                                       unsigned int *exit_status);

#ifdef __cplusplus
}
#endif

#endif

#ifndef REPROC_ERROR_H
#define REPROC_ERROR_H

#include "export.h"

/* When editing make sure to change the corresponding enum in error.hpp as
well */
typedef enum {
  /*!
  Indicates a library call was successful. Because it evaluates to zero it can
  be used as follows:

  \code{.c}
  REPROC_ERROR error = reproc_read(...);
  if (error) { return error; } // Only executes if reproc_read returns error
  \endcode

  All library function return this value if no error occurs.
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
  /*! The current or child process was not allowed to create any more pipes. */
  REPROC_PIPE_LIMIT_REACHED,
  /*! A waiting system call (read, write, wait, ...) was interrupted by the
  system. */
  REPROC_INTERRUPTED,
  /*! The current process was not allowed to spawn any more child processes. */
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
  /*! The given name was too long (most systems have path length limits) */
  REPROC_NAME_TOO_LONG,
  /*! Only part of the buffer was written to the stdin of the child process */
  REPROC_PARTIAL_WRITE
} REPROC_ERROR;

#ifdef __cplusplus
extern "C" {
#endif

/*!
Returns the last system error code.

On Windows this function returns the result of GetLastError. On POSIX platforms
this function returns the value of errno. The value is not stored so other
functions that modify the results of GetLastError or errno should not be
called if you want to retrieve the last system error that occurred in one of
reproc's functions.

On POSIX, if an error occurs after fork but before exec it is communicated to
the parent process which sets its own errno value to the errno value of the
child process. This makes it possible to retrieve errors that happen after
forking with this function (for example in chdir or execve).

\return unsigned int The last system error code
*/
REPROC_EXPORT unsigned int reproc_system_error(void);

/*! Returns a string representation of /p error. The returned string does not
have to be freed. */
REPROC_EXPORT const char *reproc_error_to_string(REPROC_ERROR error);

#ifdef __cplusplus
}
#endif

#endif

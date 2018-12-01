#ifndef REPROC_ERROR_H
#define REPROC_ERROR_H

#include <reproc/export.h>

/*!
reproc's error enum. Contains all errors that may be returned by reproc's API.
Because `REPROC_SUCCESS` evaluates to zero it is possible to check for errors as
follows:

```c
REPROC_ERROR error = reproc_read(...);
if (error) { return error; } // Only executes if reproc_read returns an error.
```
*/
// When editing make sure to change the corresponding enum in error.hpp as well.
typedef enum {
  /*! Indicates a reproc API call was successful. An API function return this
  value if no error occurs during its execution. */
  REPROC_SUCCESS = 0,

  // reproc errors: these errors do not correspond to a system error.

  /*! A timeout value passed to an API function expired. */
  REPROC_WAIT_TIMEOUT = 1,
  /*! The child process closed one of its streams (and in the case of
  stdout/stderr all of the data from that stream has been read). */
  REPROC_STREAM_CLOSED = 2,
  /*! Only part of the buffer was written to the stdin of the child process. */
  REPROC_PARTIAL_WRITE = 3,

  // system errors: these errors correspond to a system error.

  /*! A memory allocation inside reproc failed (Windows only) or the operating
  system did not have enough memory to execute a system call succesfully. */
  REPROC_NOT_ENOUGH_MEMORY = 4,
  /*! The parent or child process was not allowed to create any more pipes. */
  REPROC_PIPE_LIMIT_REACHED = 5,
  /*! A blocking system call (read, write, wait, ...) was interrupted by a
  signal. */
  REPROC_INTERRUPTED = 6,
  /*! The parent process was not allowed to spawn any more child processes. */
  REPROC_PROCESS_LIMIT_REACHED = 7,
  /*! An UTF-8 string did not contain valid unicode. */
  REPROC_INVALID_UNICODE = 8,
  /*! The parent process does not have permission to execute the program. */
  REPROC_PERMISSION_DENIED = 9,
  /*! Too many symlinks were encountered while looking for the program. */
  REPROC_SYMLINK_LOOP = 10,
  /*! The program or working directory was not found. */
  REPROC_FILE_NOT_FOUND = 11,
  /*! The given name or path was too long (most systems have path length
  limits). */
  REPROC_NAME_TOO_LONG = 12,
  /*! The given argument list was too long (some systems limit the size of
  argv). */
  REPROC_ARGS_TOO_LONG = 13,
  /*! The system does not support executing the program. */
  REPROC_NOT_EXECUTABLE = 14,
  /*! Unlike POSIX, Windows does not include information about exactly which
  errors can occur in its documentation. If an error occurs that is not known
  functions will return `REPROC_UNKNOWN_ERROR`. */
  REPROC_UNKNOWN_ERROR = 15,
} REPROC_ERROR;

#ifdef __cplusplus
extern "C" {
#endif

/*!
Returns the last system error code.

On Windows this function returns the result of `GetLastError`. On POSIX systems
this function returns the value of `errno`. The value is not saved so other
functions that modify the results of `GetLastError` or `errno` should not be
called if you want to retrieve the last system error that occurred in one of
reproc's API functions.

On POSIX systems, if an error occurs after `fork` but before `exec` it is
communicated to the parent process which sets its own `errno` value to the
`errno` value of the child process. This makes it possible to retrieve errors
that happen after forking with this function (for example in `chdir` or `exec`).
*/
REPROC_EXPORT unsigned int reproc_system_error(void);

/*! Returns a string describing `error`. This string must not be modified by the
caller. */
REPROC_EXPORT const char *reproc_strerror(REPROC_ERROR error);

#ifdef __cplusplus
}
#endif

#endif

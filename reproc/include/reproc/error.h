#pragma once

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
  REPROC_SUCCESS,

  // reproc-specific errors. These are not necessarily fatal errors and should
  // be handled separately.

  /*! A timeout value passed to an API function expired. */
  REPROC_ERROR_WAIT_TIMEOUT,
  /*! The child process closed one of its streams (and in the case of
  stdout/stderr all of the data from that stream has been read). */
  REPROC_ERROR_STREAM_CLOSED,
  /*! Only part of the buffer was written to the stdin of the child process. */
  REPROC_ERROR_PARTIAL_WRITE,

  // A system error occurred. This is usually a fatal error.
  REPROC_ERROR_SYSTEM
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

/*!
Returns a string describing `error`. This string must not be modified by the
caller.

This function is not thread-safe.
*/
REPROC_EXPORT const char *reproc_strerror(REPROC_ERROR error);

#ifdef __cplusplus
}
#endif

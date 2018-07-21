#ifndef REPROC_ERROR_H
#define REPROC_ERROR_H

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

#endif

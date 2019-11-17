#pragma once

#include <reproc/error.h>
#include <reproc/export.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! Used to store information about a child process. `reproc_t` is an opaque
type and can be allocated and freed via `reproc_new` and `reproc_free`
respectively. */
typedef struct reproc_t reproc_t;

/*! Used to tell reproc where to redirect the streams of the child process. */
typedef enum {
  /*! Redirect the stream to a pipe. */
  REPROC_REDIRECT_PIPE,
  /*! Inherit the corresponding stream from the parent process. */
  REPROC_REDIRECT_INHERIT,
  /*! Redirect the stream to /dev/null (or NUL on Windows). */
  REPROC_REDIRECT_DISCARD
} REPROC_REDIRECT;

/*! Used to tell `reproc_stop` how to stop a child process. */
typedef enum {
  /*! noop (no operation) */
  REPROC_STOP_NOOP,
  /*! `reproc_wait` */
  REPROC_STOP_WAIT,
  /*! `reproc_terminate` */
  REPROC_STOP_TERMINATE,
  /*! `reproc_kill` */
  REPROC_STOP_KILL,
} REPROC_STOP;

typedef struct reproc_stop_action {
  REPROC_STOP action;
  unsigned int timeout;
} reproc_stop_action;

typedef struct reproc_stop_actions {
  reproc_stop_action first;
  reproc_stop_action second;
  reproc_stop_action third;
} reproc_stop_actions;

typedef struct reproc_options {
  /*!
  `environment` is an array of UTF-8 encoded, null terminated strings that
  specifies the environment for the child process. It has the following layout:

  - All elements except the final element must be of the format `NAME=VALUE`.
  - The final element must be `NULL`.

  Example: ["IP=127.0.0.1", "PORT=8080", `NULL`]

  If `environment` is `NULL`, the child process inherits the environment of the
  current process.
  */
  const char *const *environment;
  /*!
  `working_directory` specifies the working directory for the child process. If
  `working_directory` is `NULL`, the child process runs in the working directory
  of the parent process.
  */
  const char *working_directory;
  /*!
  `redirect` specifies where to redirect the streams from the child process.

  By default each stream is redirected to a pipe which can be written to (stdin)
  or read from (stdout/stderr) using `reproc_write` and `reproc_read`
  respectively.
  */
  struct {
    REPROC_REDIRECT in;
    REPROC_REDIRECT out;
    REPROC_REDIRECT err;
  } redirect;
  /*!
  Stop actions that are passed to `reproc_stop` in `reproc_destroy` to stop the
  child process.

  When `stop_actions` is default-initialized (3x noop with 0 timeout),
  `reproc_destroy` will default to waiting indefinitely for the child process to
  exit.
  */
  reproc_stop_actions stop_actions;
} reproc_options;

/*! Stream identifiers used to indicate which stream to act on. */
typedef enum {
  /*! stdin */
  REPROC_STREAM_IN,
  /*! stdout */
  REPROC_STREAM_OUT,
  /*! stderr */
  REPROC_STREAM_ERR
} REPROC_STREAM;

/*! Tells a function that takes a timeout value to wait indefinitely. */
REPROC_EXPORT extern const unsigned int REPROC_INFINITE;

/* Allocate a new `reproc_t` instance on the heap. */
REPROC_EXPORT reproc_t *reproc_new(void);

/*!
Starts the process specified by `argv` in the given working directory and
redirects its input, output and error streams.

If this function does not return an error the child process will have started
running and can be inspected using the operating system's tools for process
inspection (e.g. ps on Linux).

Every successful call to this function should be followed by a successful call
to `reproc_wait` or `reproc_stop` and a call to `reproc_destroy`. If an error
occurs during `reproc_start` all allocated resources are cleaned up before
`reproc_start` returns and no further action is required.

`argv` is an array of UTF-8 encoded, null terminated strings that specifies the
program to execute along with its arguments. It has the following layout:

- The first element indicates the executable to run as a child process. This can
be an absolute path, a path relative to the working directory of the parent
process or the name of an executable located in the PATH. It cannot be `NULL`.
- The following elements indicate the whitespace delimited arguments passed to
the executable. None of these elements can be `NULL`.
- The final element must be `NULL`.

Example: ["cmake", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", `NULL`]

Possible errors:
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_start(reproc_t *process,
                                        const char *const *argv,
                                        reproc_options options);

/*!
Reads up to `size` bytes from either the child process stdout or stderr stream
and stores them them in `buffer`. The amount of bytes read is stored in
`bytes_read`.

If `stream` is not `NULL`, it is used to store the stream that was
read from (`REPROC_STREAM_OUT` or `REPROC_STREAM_ERR`).

Assuming no other errors occur this function will return `REPROC_SUCCESS` until
the stream is closed and all remaining data has been read.

It is undefined behaviour to call this function on child process streams that
were not specified as `REPROC_REDIRECT_PIPE` in the options passed to
`reproc_start`.

Possible errors:
- `REPROC_ERROR_STREAM_CLOSED`
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_read(reproc_t *process,
                                       REPROC_STREAM *stream,
                                       uint8_t *buffer,
                                       unsigned int size,
                                       unsigned int *bytes_read);

/*!
Calls `reproc_read` on `stream` until `parser` returns false or an error occurs.
`parser` receives the output after each read, along with `context`.

`reproc_parse` always starts by calling `parser` once with an empty buffer and
`stream` set to `REPROC_STREAM_IN` to give the parser the chance to process all
output from the previous call to `reproc_parse` one by one.

Possible errors:
- `REPROC_ERROR_STREAM_CLOSED`
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_parse(reproc_t *process,
                                        bool (*parser)(REPROC_STREAM stream,
                                                       const uint8_t *buffer,
                                                       unsigned int size,
                                                       void *context),
                                        void *context);

/*!
`reproc_parse` but `REPROC_ERROR_STREAM_CLOSED` is not treated as an error.

For examples of sinks, see `sink.h`.

Possible errors:
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_drain(reproc_t *process,
                                        bool (*sink)(REPROC_STREAM stream,
                                                     const uint8_t *buffer,
                                                     unsigned int size,
                                                     void *context),
                                        void *context);

/*!
Writes `size` bytes from `buffer` to the standard input (stdin) of the child
process.

(POSIX) By default, writing to a closed stdin pipe terminates the parent process
with the `SIGPIPE` signal. `reproc_write` will only return
`REPROC_ERROR_STREAM_CLOSED` if this signal is ignored by the parent process.

It is undefined behaviour to call this function on a process whose stdin stream
was not specified as `REPROC_REDIRECT_PIPE` in the options passed to
`reproc_start`.

Possible errors:
- `REPROC_ERROR_STREAM_CLOSED`
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_write(reproc_t *process,
                                        const uint8_t *buffer,
                                        unsigned int size);

/*!
Closes the stream endpoint of the parent process indicated by `stream`.

This function is necessary when a child process reads from stdin until it is
closed. After writing all the input to the child process with `reproc_write`,
the standard input stream can be closed using this function.
*/
REPROC_EXPORT void reproc_close(reproc_t *process, REPROC_STREAM stream);

/*!
Waits `timeout` milliseconds for the child process to exit.

If `timeout` is 0 the function will only check if the child process is still
running without waiting. If `timeout` is `REPROC_INFINITE` the function will
wait indefinitely for the child process to exit.

If this function returns `REPROC_SUCCESS`, `process` has exited and its exit
status can be retrieved with `reproc_exit_status`.

Possible errors:
- `REPROC_ERROR_WAIT_TIMEOUT`
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_wait(reproc_t *process, unsigned int timeout);

/*! Returns `true` if `process` is still running, `false` otherwise. */
REPROC_EXPORT bool reproc_running(reproc_t *process);

/*!
Sends the `SIGTERM` signal (POSIX) or the `CTRL-BREAK` signal (Windows) to the
child process. Remember that successfull calls to `reproc_wait` and
`reproc_destroy` are required to make sure the child process is completely
cleaned up.

Possible errors:
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_terminate(reproc_t *process);

/*!
Sends the `SIGKILL` signal to the child process (POSIX) or calls
`TerminateProcess` (Windows) on the child process. Remember that successfull
calls to `reproc_wait` and `reproc_destroy` are required to make sure the child
process is completely cleaned up.

Possible errors:
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_kill(reproc_t *process);

/*!
Simplifies calling combinations of `reproc_wait`, `reproc_terminate` and
`reproc_kill`. The function executes each specified step and waits (using
`reproc_wait`) until the corresponding timeout expires before continuing with
the next step.

Example:

Wait 10 seconds for the child process to exit on its own before sending
`SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waiting five more seconds for
the child process to exit.

```c
REPROC_ERROR error = reproc_stop(process,
                                 REPROC_STOP_WAIT, 10000,
                                 REPROC_STOP_TERMINATE, 5000,
                                 REPROC_STOP_NOOP, 0);
```

Call `reproc_wait`, `reproc_terminate` and `reproc_kill` directly if you need
extra logic such as logging between calls.

(`c1`,`t1`), (`c2`,`t2`) and (`c3`,`t3`) are pairs that instruct `reproc_stop`
how to stop a process. The first element of each pair instructs `reproc_stop` to
execute the corresponding function to stop the process. The second element of
each pair tells `reproc_stop` how long to wait after executing the function
indicated by the first element.

If this function returns `REPROC_SUCCESS`, `process` has exited and its exit
status can be retrieved with `reproc_exit_status`.

Possible errors:
- `REPROC_ERROR_WAIT_TIMEOUT`
- `REPROC_ERROR_SYSTEM`
*/
REPROC_EXPORT REPROC_ERROR reproc_stop(reproc_t *process,
                                       reproc_stop_actions stop_actions);

/*! Returns the exit status of `process` if `process` has exited. Returns
`-1` `process` has not been started or if `process` is still running. */
REPROC_EXPORT int reproc_exit_status(reproc_t *process);

/*! Release all resources associated with `process` including the memory
allocated by `reproc_new`. Calling this function before a succesfull call to
`reproc_wait` can result in resource leaks. */
REPROC_EXPORT void reproc_destroy(reproc_t *process);

#ifdef __cplusplus
}
#endif

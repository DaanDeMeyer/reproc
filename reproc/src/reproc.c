#include <reproc/reproc.h>

#include "error.h"
#include "handle.h"
#include "macro.h"
#include "pipe.h"
#include "process.h"
#include "redirect.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>

struct reproc_t {
  int exit_status;

  handle handle;
  handle in;
  handle out;
  handle err;

  reproc_stop_actions stop_actions;
};

const unsigned int REPROC_INFINITE = 0xFFFFFFFF; // == `INFINITE` on Windows

static int redirect(handle *parent,
                    handle *child,
                    REPROC_STREAM stream,
                    REPROC_REDIRECT type)
{
  switch (type) {

    case REPROC_REDIRECT_PIPE:
      return redirect_pipe(parent, child, (REDIRECT_STREAM) stream);

    case REPROC_REDIRECT_INHERIT:;
      int r = redirect_inherit(parent, child, (REDIRECT_STREAM) stream);
      // Discard if the corresponding parent stream is closed.
      return r == REPROC_ERROR_STREAM_CLOSED
                 ? redirect_discard(parent, child, (REDIRECT_STREAM) stream)
                 : r;

    case REPROC_REDIRECT_DISCARD:
      return redirect_discard(parent, child, (REDIRECT_STREAM) stream);
  }

  return REPROC_ERROR_INVALID_ARGUMENT;
}

#define assert_return(expression, r)                                           \
  do {                                                                         \
    if (!(expression)) {                                                       \
      return (r);                                                              \
    }                                                                          \
  } while (false)

reproc_t *reproc_new(void)
{
  reproc_t *process = malloc(sizeof(reproc_t));
  if (process == NULL) {
    return NULL;
  }

  *process = (reproc_t){ .exit_status = REPROC_ERROR_INVALID_ARGUMENT,
                         .handle = HANDLE_INVALID,
                         .in = HANDLE_INVALID,
                         .out = HANDLE_INVALID,
                         .err = HANDLE_INVALID };

  return process;
}

int reproc_start(reproc_t *process,
                 const char *const *argv,
                 reproc_options options)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(process->exit_status == REPROC_ERROR_INVALID_ARGUMENT,
                REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(argv, REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(argv[0], REPROC_ERROR_INVALID_ARGUMENT);

  handle child_in = HANDLE_INVALID;
  handle child_out = HANDLE_INVALID;
  handle child_err = HANDLE_INVALID;

  int r = -1;

  r = redirect(&process->in, &child_in, REPROC_STREAM_IN, options.redirect.in);
  if (r < 0) {
    goto cleanup;
  }

  r = redirect(&process->out, &child_out, REPROC_STREAM_OUT,
               options.redirect.out);
  if (r < 0) {
    goto cleanup;
  }

  r = redirect(&process->err, &child_err, REPROC_STREAM_ERR,
               options.redirect.err);
  if (r < 0) {
    goto cleanup;
  }

  struct process_options process_options = {
    .environment = options.environment,
    .working_directory = options.working_directory,
    .redirect = { .in = child_in, .out = child_out, .err = child_err }
  };

  r = process_create(&process->handle, argv, process_options);
  if (r < 0) {
    goto cleanup;
  }

  process->stop_actions = options.stop_actions;

  bool is_noop = process->stop_actions.first.action == REPROC_STOP_NOOP &&
                 process->stop_actions.second.action == REPROC_STOP_NOOP &&
                 process->stop_actions.third.action == REPROC_STOP_NOOP;

  if (is_noop) {
    process->stop_actions.first.action = REPROC_STOP_WAIT;
    process->stop_actions.first.timeout = REPROC_INFINITE;
  }

cleanup:
  // Either an error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way, they can
  // be safely closed in the parent process.
  handle_destroy(child_in);
  handle_destroy(child_out);
  handle_destroy(child_err);

  if (r < 0) {
    process->handle = process_destroy(process->handle);
    process->in = handle_destroy(process->in);
    process->out = handle_destroy(process->out);
    process->err = handle_destroy(process->err);
  } else {
    process->exit_status = REPROC_ERROR_IN_PROGRESS;
  }

  return r;
}

int reproc_read(reproc_t *process,
                REPROC_STREAM *stream,
                uint8_t *buffer,
                size_t size)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(buffer, REPROC_ERROR_INVALID_ARGUMENT);

  handle pipes[2] = { process->err, process->out };
  int r = -1;

  r = pipe_wait(pipes, ARRAY_SIZE(pipes));
  if (r < 0) {
    return r;
  }

  int ready = r;

  r = pipe_read(pipes[ready], buffer, size);
  if (r < 0) {
    return r;
  }

  int bytes_read = r;

  if (stream) {
    *stream = pipes[ready] == process->out ? REPROC_STREAM_OUT
                                           : REPROC_STREAM_ERR;
  }

  return bytes_read;
}

int reproc_drain(reproc_t *process,
                 bool (*sink)(REPROC_STREAM stream,
                              const uint8_t *buffer,
                              size_t size,
                              void *context),
                 void *context)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(sink, REPROC_ERROR_INVALID_ARGUMENT);

  // A single call to `read` might contain multiple messages. By always calling
  // `sink` once with no data before reading, we give it the chance to process
  // all previous output one by one before reading from the child process again.
  if (!sink(REPROC_STREAM_IN, (uint8_t[]){ 0 }, 0, context)) {
    return 0;
  }

  uint8_t buffer[4096];
  int r = -1;

  while (true) {
    REPROC_STREAM stream = { 0 };
    r = reproc_read(process, &stream, buffer, ARRAY_SIZE(buffer));
    if (r < 0) {
      break;
    }

    size_t bytes_read = (size_t) r;

    // `sink` returns false to tell us to stop reading.
    if (!sink(stream, buffer, bytes_read, context)) {
      break;
    }
  }

  return r == REPROC_ERROR_STREAM_CLOSED ? 0 : r;
}

int reproc_write(reproc_t *process, const uint8_t *buffer, size_t size)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(process->in, REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(process->exit_status == REPROC_ERROR_IN_PROGRESS,
                REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(buffer, REPROC_ERROR_INVALID_ARGUMENT);

  int r = -1;

  do {
    r = pipe_write(process->in, buffer, size);
    if (r < 0) {
      break;
    }

    size_t bytes_written = (size_t) r;

    assert(bytes_written <= size);
    buffer += bytes_written;
    size -= bytes_written;
  } while (size != 0);

  return r < 0 ? r : 0;
}

int reproc_close(reproc_t *process, REPROC_STREAM stream)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);
  assert_return(process->exit_status == REPROC_ERROR_IN_PROGRESS,
                REPROC_ERROR_INVALID_ARGUMENT);

  switch (stream) {
    case REPROC_STREAM_IN:
      process->in = handle_destroy(process->in);
      break;
    case REPROC_STREAM_OUT:
      process->out = handle_destroy(process->out);
      break;
    case REPROC_STREAM_ERR:
      process->err = handle_destroy(process->err);
      break;
    default:
      return REPROC_ERROR_INVALID_ARGUMENT;
  }

  return 0;
}

int reproc_wait(reproc_t *process, unsigned int timeout)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);

  int r = -1;

  if (process->exit_status != REPROC_ERROR_IN_PROGRESS) {
    return 0;
  }

  r = process_wait(process->handle, timeout);
  if (r < 0) {
    return r;
  }

  process->exit_status = r;

  return 0;
}

bool reproc_running(reproc_t *process)
{
  return reproc_wait(process, 0) == REPROC_ERROR_WAIT_TIMEOUT;
}

int reproc_terminate(reproc_t *process)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);

  if (!reproc_running(process)) {
    return 0;
  }

  return process_terminate(process->handle);
}

int reproc_kill(reproc_t *process)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);

  if (!reproc_running(process)) {
    return 0;
  }

  return process_kill(process->handle);
}

int reproc_stop(reproc_t *process, reproc_stop_actions stop_actions)
{
  assert_return(process, REPROC_ERROR_INVALID_ARGUMENT);

  reproc_stop_action actions[3] = { stop_actions.first, stop_actions.second,
                                    stop_actions.third };
  int r = -1;

  for (size_t i = 0; i < ARRAY_SIZE(actions); i++) {
    switch (actions[i].action) {
      case REPROC_STOP_NOOP:
        continue;
      case REPROC_STOP_WAIT:
        r = 0;
        break;
      case REPROC_STOP_TERMINATE:
        r = reproc_terminate(process);
        break;
      case REPROC_STOP_KILL:
        r = reproc_kill(process);
        break;
      default:
        return REPROC_ERROR_INVALID_ARGUMENT;
    }

    // Stop if `reproc_terminate` or `reproc_kill` fail.
    if (r < 0) {
      break;
    }

    r = reproc_wait(process, actions[i].timeout);
    if (r != REPROC_ERROR_WAIT_TIMEOUT) {
      break;
    }
  }

  return r;
}

int reproc_exit_status(reproc_t *process)
{
  return process->exit_status;
}

reproc_t *reproc_destroy(reproc_t *process)
{
  assert_return(process, NULL);

  reproc_stop(process, process->stop_actions);

  process->handle = process_destroy(process->handle);
  process->in = handle_destroy(process->in);
  process->out = handle_destroy(process->out);
  process->err = handle_destroy(process->err);

  free(process);
  return NULL;
}

const char *reproc_error_string(int error)
{
  return error_string(error);
}

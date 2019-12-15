#include <reproc/reproc.h>

#include <handle.h>
#include <macro.h>
#include <pipe.h>
#include <process.h>
#include <redirect.h>

#include <assert.h>
#include <stdlib.h>

enum { REPROC_STATUS_NOT_STARTED = -1, REPROC_STATUS_RUNNING = -2 };

struct reproc_t {
  int exit_status;

  reproc_handle handle;
  reproc_handle in;
  reproc_handle out;
  reproc_handle err;

  reproc_stop_actions stop_actions;
};

const unsigned int REPROC_INFINITE = 0xFFFFFFFF; // == `INFINITE` on Windows

static REPROC_ERROR redirect(reproc_handle *parent,
                             reproc_handle *child,
                             REPROC_STREAM stream,
                             REPROC_REDIRECT type)
{
  switch (type) {

    case REPROC_REDIRECT_PIPE:
      return redirect_pipe(parent, child, stream);

    case REPROC_REDIRECT_INHERIT:;
      REPROC_ERROR error = redirect_inherit(parent, child, stream);
      // Discard if the corresponding parent stream is closed.
      return error == REPROC_ERROR_STREAM_CLOSED
                 ? redirect_discard(parent, child, stream)
                 : error;

    case REPROC_REDIRECT_DISCARD:
      return redirect_discard(parent, child, stream);
  }

  assert(false);
  return REPROC_ERROR_SYSTEM;
}

reproc_t *reproc_new(void)
{
  reproc_t *process = malloc(sizeof(reproc_t));
  if (process == NULL) {
    return NULL;
  }

  *process = (reproc_t){ .exit_status = REPROC_STATUS_NOT_STARTED };

  return process;
}

REPROC_ERROR
reproc_start(reproc_t *process, const char *const *argv, reproc_options options)
{
  assert(process);
  assert(process->exit_status == REPROC_STATUS_NOT_STARTED);
  assert(argv);
  assert(argv[0]);

  reproc_handle child_in = 0;
  reproc_handle child_out = 0;
  reproc_handle child_err = 0;

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  error = redirect(&process->in, &child_in, REPROC_STREAM_IN,
                   options.redirect.in);
  if (error) {
    goto cleanup;
  }

  error = redirect(&process->out, &child_out, REPROC_STREAM_OUT,
                   options.redirect.out);
  if (error) {
    goto cleanup;
  }

  error = redirect(&process->err, &child_err, REPROC_STREAM_ERR,
                   options.redirect.err);
  if (error) {
    goto cleanup;
  }

  struct process_options process_options = {
    .environment = options.environment,
    .working_directory = options.working_directory,
    .redirect = { .in = child_in, .out = child_out, .err = child_err }
  };

  error = process_create(&process->handle, argv, process_options);
  if (error) {
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

  error = REPROC_SUCCESS;

cleanup:
  // Either an error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way, they can
  // be safely closed in the parent process.
  handle_close(&child_in);
  handle_close(&child_out);
  handle_close(&child_err);

  if (error) {
    reproc_destroy(process);
  } else {
    process->exit_status = REPROC_STATUS_RUNNING;
  }

  return error;
}

REPROC_ERROR reproc_read(reproc_t *process,
                         REPROC_STREAM *stream,
                         uint8_t *buffer,
                         unsigned int size,
                         unsigned int *bytes_read)
{
  assert(process);
  assert(process->exit_status == REPROC_STATUS_RUNNING);
  assert(buffer);
  assert(bytes_read);

  reproc_handle pipes[2] = { process->err, process->out };
  unsigned int ready = 0;

  REPROC_ERROR error = pipe_wait(pipes, ARRAY_SIZE(pipes), &ready);
  if (error) {
    return error;
  }

  error = pipe_read(pipes[ready], buffer, size, bytes_read);
  if (error) {
    return error;
  }

  if (stream) {
    *stream = pipes[ready] == process->out ? REPROC_STREAM_OUT
                                           : REPROC_STREAM_ERR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_parse(reproc_t *process,
                          bool (*parser)(REPROC_STREAM stream,
                                         const uint8_t *buffer,
                                         unsigned int size,
                                         void *context),
                          void *context)
{
  assert(process);
  assert(process->exit_status == REPROC_STATUS_RUNNING);
  assert(parser);

  // A single call to `read` might contain multiple messages. By always calling
  // `parser` once with no data before reading, we give it the chance to process
  // all previous output one by one before reading from the child process again.
  if (!parser(REPROC_STREAM_IN, (uint8_t[]){ 0 }, 0, context)) {
    return REPROC_SUCCESS;
  }

  uint8_t buffer[4096];
  REPROC_ERROR error = REPROC_SUCCESS;

  while (true) {
    REPROC_STREAM stream = { 0 };
    unsigned int bytes_read = 0;
    error = reproc_read(process, &stream, buffer, ARRAY_SIZE(buffer),
                        &bytes_read);
    if (error) {
      break;
    }

    // `parser` returns false to tell us to stop reading.
    if (!parser(stream, buffer, bytes_read, context)) {
      break;
    }
  }

  return error;
}

REPROC_ERROR
reproc_drain(reproc_t *process,
             bool (*sink)(REPROC_STREAM stream,
                          const uint8_t *buffer,
                          unsigned int size,
                          void *context),
             void *context)
{
  assert(process);
  assert(process->exit_status == REPROC_STATUS_RUNNING);
  assert(sink);

  REPROC_ERROR error = reproc_parse(process, sink, context);

  return error == REPROC_ERROR_STREAM_CLOSED ? REPROC_SUCCESS : error;
}

REPROC_ERROR
reproc_write(reproc_t *process, const uint8_t *buffer, unsigned int size)
{
  assert(process);
  assert(process->in);
  assert(process->exit_status == REPROC_STATUS_RUNNING);
  assert(buffer);

  do {
    unsigned int bytes_written = 0;

    REPROC_ERROR error = pipe_write(process->in, buffer, size, &bytes_written);
    if (error) {
      return error;
    }

    assert(bytes_written <= size);
    buffer += bytes_written;
    size -= bytes_written;
  } while (size != 0);

  return REPROC_SUCCESS;
}

void reproc_close(reproc_t *process, REPROC_STREAM stream)
{
  assert(process);
  assert(process->exit_status == REPROC_STATUS_RUNNING);

  switch (stream) {
    case REPROC_STREAM_IN:
      handle_close(&process->in);
      return;
    case REPROC_STREAM_OUT:
      handle_close(&process->out);
      return;
    case REPROC_STREAM_ERR:
      handle_close(&process->err);
      return;
  }

  assert(false);
}

REPROC_ERROR reproc_wait(reproc_t *process, unsigned int timeout)
{
  assert(process);

  if (process->exit_status != REPROC_STATUS_RUNNING) {
    return REPROC_SUCCESS;
  }

  unsigned int completed = 0;
  REPROC_ERROR error = process_wait(&process->handle, 1, timeout, &completed,
                                    &process->exit_status);
  if (error) {
    return error;
  }

  assert(completed == 1);

  return REPROC_SUCCESS;
}

bool reproc_running(reproc_t *process)
{
  assert(process);

  return reproc_wait(process, 0) == REPROC_ERROR_WAIT_TIMEOUT;
}

REPROC_ERROR reproc_terminate(reproc_t *process)
{
  assert(process);

  if (!reproc_running(process)) {
    return REPROC_SUCCESS;
  }

  return process_terminate(process->handle);
}

REPROC_ERROR reproc_kill(reproc_t *process)
{
  assert(process);

  if (!reproc_running(process)) {
    return REPROC_SUCCESS;
  }

  return process_kill(process->handle);
}

REPROC_ERROR reproc_stop(reproc_t *process, reproc_stop_actions stop_actions)
{
  assert(process);

  reproc_stop_action actions[3] = { stop_actions.first, stop_actions.second,
                                    stop_actions.third };

  // We don't set `error` to `REPROC_SUCCESS` so we can check if `reproc_wait`,
  // `reproc_terminate` or `reproc_kill` succeed (in which case `error` is set
  // to `REPROC_SUCCESS`).
  REPROC_ERROR error = REPROC_ERROR_WAIT_TIMEOUT;

  for (unsigned int i = 0; i < ARRAY_SIZE(actions); i++) {
    switch (actions[i].action) {
      case REPROC_STOP_NOOP:
        error = REPROC_SUCCESS;
        continue;
      case REPROC_STOP_WAIT:
        break;
      case REPROC_STOP_TERMINATE:
        error = reproc_terminate(process);
        break;
      case REPROC_STOP_KILL:
        error = reproc_kill(process);
        break;
    }

    // Stop if `reproc_terminate` or `reproc_kill` return an error.
    if (error != REPROC_SUCCESS && error != REPROC_ERROR_WAIT_TIMEOUT) {
      break;
    }

    error = reproc_wait(process, actions[i].timeout);
    if (error != REPROC_ERROR_WAIT_TIMEOUT) {
      break;
    }
  }

  return error;
}

int reproc_exit_status(reproc_t *process)
{
  assert(process);

  return process->exit_status >= 0 ? process->exit_status : -1;
}

void reproc_destroy(reproc_t *process)
{
  assert(process);

  reproc_stop(process, process->stop_actions);

  // Process handle only needs to be closed on Windows. `waitpid` takes care of
  // it for us on POSIX.
#if defined(_WIN32)
  handle_close(&process->handle);
#endif

  handle_close(&process->in);
  handle_close(&process->out);
  handle_close(&process->err);

  free(process);
}

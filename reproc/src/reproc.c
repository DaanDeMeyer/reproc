#include <reproc/reproc.h>

#include "error.h"
#include "handle.h"
#include "macro.h"
#include "pipe.h"
#include "process.h"
#include "redirect.h"

#include <assert.h>
#include <stdlib.h>

struct reproc_t {
  handle handle;
  struct stdio stdio;
  int status;
  reproc_stop_actions stop_actions;
};

enum { STATUS_NOT_STARTED = -2, STATUS_IN_PROGRESS = -1 };

const int REPROC_SIGKILL = UINT8_MAX + 9;
const int REPROC_SIGTERM = UINT8_MAX + 15;

const unsigned int REPROC_INFINITE = 0xFFFFFFFF; // == `INFINITE` on Windows

static int redirect(handle *parent,
                    handle *child,
                    REPROC_STREAM stream,
                    REPROC_REDIRECT type)
{
  assert(parent);
  assert(child);

  int r = -1;

  switch (type) {

    case REPROC_REDIRECT_PIPE:
      r = redirect_pipe(parent, child, (REDIRECT_STREAM) stream);
      break;

    case REPROC_REDIRECT_INHERIT:;
      r = redirect_inherit(parent, child, (REDIRECT_STREAM) stream);
      // Discard if the corresponding parent stream is closed.
      r = r == REPROC_EPIPE
              ? redirect_discard(parent, child, (REDIRECT_STREAM) stream)
              : r;
      break;

    case REPROC_REDIRECT_DISCARD:
      r = redirect_discard(parent, child, (REDIRECT_STREAM) stream);
      break;

    default:
      r = REPROC_EINVAL;
      break;
  }

  return r;
}

reproc_t *reproc_new(void)
{
  reproc_t *process = malloc(sizeof(reproc_t));
  if (process == NULL) {
    return NULL;
  }

  *process = (reproc_t){ .handle = HANDLE_INVALID,
                         .stdio = { .in = HANDLE_INVALID,
                                    .out = HANDLE_INVALID,
                                    .err = HANDLE_INVALID },
                         .status = STATUS_NOT_STARTED };

  return process;
}

int reproc_start(reproc_t *process,
                 const char *const *argv,
                 reproc_options options)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(process->status == STATUS_NOT_STARTED, REPROC_EINVAL);
  assert_return(argv, REPROC_EINVAL);
  assert_return(argv[0], REPROC_EINVAL);

  struct stdio child = { HANDLE_INVALID, HANDLE_INVALID, HANDLE_INVALID };

  int r = -1;

  r = redirect(&process->stdio.in, &child.in, REPROC_STREAM_IN,
               options.redirect.in);
  if (r < 0) {
    goto cleanup;
  }

  r = redirect(&process->stdio.out, &child.out, REPROC_STREAM_OUT,
               options.redirect.out);
  if (r < 0) {
    goto cleanup;
  }

  r = redirect(&process->stdio.err, &child.err, REPROC_STREAM_ERR,
               options.redirect.err);
  if (r < 0) {
    goto cleanup;
  }

  struct process_options process_options = {
    .environment = options.environment,
    .working_directory = options.working_directory,
    .redirect = child
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
  handle_destroy(child.in);
  handle_destroy(child.out);
  handle_destroy(child.err);

  if (r < 0) {
    process->handle = process_destroy(process->handle);
    process->stdio.in = handle_destroy(process->stdio.in);
    process->stdio.out = handle_destroy(process->stdio.out);
    process->stdio.err = handle_destroy(process->stdio.err);
  } else {
    process->status = STATUS_IN_PROGRESS;
  }

  return r;
}

int reproc_read(reproc_t *process,
                REPROC_STREAM *stream,
                uint8_t *buffer,
                size_t size)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(buffer, REPROC_EINVAL);

  int r = -1;

  while (true) {
    // Get the first pipe that will have data available to be read.
    handle ready = HANDLE_INVALID;

    if (process->stdio.out == HANDLE_INVALID &&
        process->stdio.err == HANDLE_INVALID) {
      return REPROC_EPIPE;
    } else if (process->stdio.out == HANDLE_INVALID) {
      ready = process->stdio.err;
    } else if (process->stdio.err == HANDLE_INVALID) {
      ready = process->stdio.out;
    } else {
      r = pipe_wait(process->stdio.out, process->stdio.err, &ready);
      if (r < 0) {
        return r;
      }
    }

    r = pipe_read(ready, buffer, size);
    if (r >= 0) {
      if (stream) {
        *stream = ready == process->stdio.out ? REPROC_STREAM_OUT
                                              : REPROC_STREAM_ERR;
      }

      break;
    }

    if (r != REPROC_EPIPE) {
      return r;
    }

    // If the pipe was closed, destroy its handle and try waiting again.

    if (ready == process->stdio.out) {
      process->stdio.out = handle_destroy(process->stdio.out);
    } else {
      process->stdio.err = handle_destroy(process->stdio.err);
    }
  }

  return r; // bytes read
}

int reproc_drain(reproc_t *process,
                 bool (*sink)(REPROC_STREAM stream,
                              const uint8_t *buffer,
                              size_t size,
                              void *context),
                 void *context)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(sink, REPROC_EINVAL);

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

  return r == REPROC_EPIPE ? 0 : r;
}

int reproc_write(reproc_t *process, const uint8_t *buffer, size_t size)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(buffer, REPROC_EINVAL);

  if (process->stdio.in == HANDLE_INVALID) {
    return REPROC_EPIPE;
  }

  int r = -1;

  do {
    r = pipe_write(process->stdio.in, buffer, size);

    if (r == REPROC_EPIPE) {
      process->stdio.in = handle_destroy(process->stdio.in);
    }

    if (r < 0) {
      break;
    }

    size_t bytes_written = (size_t) r;

    buffer += bytes_written;
    size -= bytes_written;
  } while (size != 0);

  return r < 0 ? r : 0;
}

int reproc_close(reproc_t *process, REPROC_STREAM stream)
{
  assert_return(process, REPROC_EINVAL);

  switch (stream) {
    case REPROC_STREAM_IN:
      process->stdio.in = handle_destroy(process->stdio.in);
      break;
    case REPROC_STREAM_OUT:
      process->stdio.out = handle_destroy(process->stdio.out);
      break;
    case REPROC_STREAM_ERR:
      process->stdio.err = handle_destroy(process->stdio.err);
      break;
    default:
      return REPROC_EINVAL;
  }

  return 0;
}

int reproc_wait(reproc_t *process, unsigned int timeout)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(process->status != STATUS_NOT_STARTED, REPROC_EINVAL);

  int r = -1;

  if (process->status >= 0) {
    return process->status;
  }

  r = process_wait(process->handle, timeout);
  if (r < 0) {
    return r;
  }

  return process->status = r;
}

int reproc_terminate(reproc_t *process)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(process->status != STATUS_NOT_STARTED, REPROC_EINVAL);

  if (process->status >= 0) {
    return 0;
  }

  return process_terminate(process->handle);
}

int reproc_kill(reproc_t *process)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(process->status != STATUS_NOT_STARTED, REPROC_EINVAL);

  if (process->status >= 0) {
    return 0;
  }

  return process_kill(process->handle);
}

int reproc_stop(reproc_t *process, reproc_stop_actions stop_actions)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(process->status != STATUS_NOT_STARTED, REPROC_EINVAL);

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
        return REPROC_EINVAL;
    }

    // Stop if `reproc_terminate` or `reproc_kill` fail.
    if (r < 0) {
      break;
    }

    r = reproc_wait(process, actions[i].timeout);
    if (r != REPROC_ETIMEDOUT) {
      break;
    }
  }

  return r;
}

reproc_t *reproc_destroy(reproc_t *process)
{
  assert_return(process, NULL);

  if (process->status == STATUS_IN_PROGRESS) {
    reproc_stop(process, process->stop_actions);
  }

  process->handle = process_destroy(process->handle);
  process->stdio.in = handle_destroy(process->stdio.in);
  process->stdio.out = handle_destroy(process->stdio.out);
  process->stdio.err = handle_destroy(process->stdio.err);

  free(process);
  return NULL;
}

const char *reproc_strerror(int error)
{
  return error_string(error);
}

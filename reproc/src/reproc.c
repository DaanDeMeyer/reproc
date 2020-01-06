#include <reproc/reproc.h>

#include "clock.h"
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
  reproc_stop_actions stop;
  int timeout;
  int64_t deadline;
};

enum { STATUS_NOT_STARTED = -2, STATUS_IN_PROGRESS = -1 };

const int REPROC_SIGKILL = UINT8_MAX + 9;
const int REPROC_SIGTERM = UINT8_MAX + 15;

const int REPROC_INFINITE = -1;
const int REPROC_DEADLINE = -2;

static int parse_options(reproc_options *options)
{
  assert(options);

  if (options->redirect.in || options->redirect.out || options->redirect.err) {
    assert_return(!options->inherit, REPROC_EINVAL);
    assert_return(!options->discard, REPROC_EINVAL);
  }

  if (options->inherit) {
    assert_return(options->redirect.in == 0, REPROC_EINVAL);
    assert_return(options->redirect.out == 0, REPROC_EINVAL);
    assert_return(options->redirect.err == 0, REPROC_EINVAL);
    assert_return(!options->discard, REPROC_EINVAL);

    options->redirect.in = REPROC_REDIRECT_INHERIT;
    options->redirect.out = REPROC_REDIRECT_INHERIT;
    options->redirect.err = REPROC_REDIRECT_INHERIT;
  }

  if (options->discard) {
    assert_return(options->redirect.in == 0, REPROC_EINVAL);
    assert_return(options->redirect.out == 0, REPROC_EINVAL);
    assert_return(options->redirect.err == 0, REPROC_EINVAL);
    assert_return(!options->inherit, REPROC_EINVAL);

    options->redirect.in = REPROC_REDIRECT_DISCARD;
    options->redirect.out = REPROC_REDIRECT_DISCARD;
    options->redirect.err = REPROC_REDIRECT_DISCARD;
  }

  if (options->input.data != NULL || options->input.size > 0) {
    assert_return(options->input.data != NULL, REPROC_EINVAL);
    assert_return(options->input.size > 0, REPROC_EINVAL);
    assert_return(options->redirect.in == 0, REPROC_EINVAL);
  }

  options->timeout = options->timeout == 0 ? REPROC_INFINITE : options->timeout;

  options->deadline = options->deadline == 0 ? REPROC_INFINITE
                                             : options->deadline;

  bool is_noop = options->stop.first.action == REPROC_STOP_NOOP &&
                 options->stop.second.action == REPROC_STOP_NOOP &&
                 options->stop.third.action == REPROC_STOP_NOOP;

  if (is_noop) {
    options->stop.first.action = REPROC_STOP_WAIT;
    options->stop.first.timeout = REPROC_DEADLINE;
    options->stop.second.action = REPROC_STOP_TERMINATE;
    options->stop.second.timeout = REPROC_INFINITE;
  }

  return 0;
}

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

static int expiry(int timeout, int64_t deadline)
{
  if (timeout == REPROC_INFINITE && deadline == REPROC_INFINITE) {
    return REPROC_INFINITE;
  }

  if (deadline == REPROC_INFINITE) {
    return timeout;
  }

  int64_t now = reproc_now();

  if (now >= deadline) {
    return 0;
  }

  // `deadline` exceeds `reproc_now` by at most a full `int` so the cast is
  // safe.
  int remaining = (int) (deadline - now);

  if (timeout == REPROC_INFINITE) {
    return remaining;
  }

  return MIN(timeout, remaining);
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
                         .status = STATUS_NOT_STARTED,
                         .timeout = REPROC_INFINITE,
                         .deadline = REPROC_INFINITE };

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

  r = parse_options(&options);
  if (r < 0) {
    return r;
  }

  r = redirect(&process->stdio.in, &child.in, REPROC_STREAM_IN,
               options.redirect.in);
  if (r < 0) {
    goto finish;
  }

  r = redirect(&process->stdio.out, &child.out, REPROC_STREAM_OUT,
               options.redirect.out);
  if (r < 0) {
    goto finish;
  }

  r = redirect(&process->stdio.err, &child.err, REPROC_STREAM_ERR,
               options.redirect.err);
  if (r < 0) {
    goto finish;
  }

  if (options.input.data != NULL) {
    // `reproc_write` only needs the child process stdin pipe to be initialized.
    r = reproc_write(process, options.input.data, options.input.size);
    if (r < 0) {
      goto finish;
    }

    r = reproc_close(process, REPROC_STREAM_IN);
    if (r < 0) {
      goto finish;
    }
  }

  struct process_options process_options = {
    .environment = options.environment,
    .working_directory = options.working_directory,
    .redirect = child
  };

  r = process_start(&process->handle, argv, process_options);
  if (r < 0) {
    goto finish;
  }

  process->stop = options.stop;
  process->timeout = options.timeout;
  process->deadline = options.deadline == REPROC_INFINITE
                          ? REPROC_INFINITE
                          : reproc_now() + options.deadline;

finish:
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

  handle ready = HANDLE_INVALID;
  int r = -1;

  while (true) {
    int timeout = expiry(process->timeout, process->deadline);
    if (timeout == 0) {
      return REPROC_ETIMEDOUT;
    }

    // Get the first pipe that will have data available to be read.
    r = pipe_wait(process->stdio.out, process->stdio.err, &ready, timeout);
    if (r < 0) {
      return r;
    }

    r = pipe_read(ready, buffer, size);
    if (r >= 0) {
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

  if (stream) {
    *stream = ready == process->stdio.out ? REPROC_STREAM_OUT
                                          : REPROC_STREAM_ERR;
  }

  return r; // bytes read
}

int reproc_write(reproc_t *process, const uint8_t *buffer, size_t size)
{
  assert_return(process, REPROC_EINVAL);

  if (buffer == NULL) {
    // Allow `NULL` buffers but only if `size == 0`.
    assert_return(size == 0, REPROC_EINVAL);
    return 0;
  }

  if (process->stdio.in == HANDLE_INVALID) {
    return REPROC_EPIPE;
  }

  int r = -1;

  do {
    int timeout = expiry(process->timeout, process->deadline);
    if (timeout == 0) {
      return REPROC_ETIMEDOUT;
    }

    r = pipe_write(process->stdio.in, buffer, size, timeout);

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

int reproc_wait(reproc_t *process, int timeout)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(process->status != STATUS_NOT_STARTED, REPROC_EINVAL);

  int r = -1;

  if (process->status >= 0) {
    return process->status;
  }

  timeout = timeout == REPROC_DEADLINE
                // If the deadline has expired, `expiry` returns 0 which means
                // we'll only check if the process is still running.
                ? expiry(REPROC_INFINITE, process->deadline)
                : timeout;

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

int reproc_stop(reproc_t *process, reproc_stop_actions stop)
{
  assert_return(process, REPROC_EINVAL);
  assert_return(process->status != STATUS_NOT_STARTED, REPROC_EINVAL);

  reproc_stop_action actions[3] = { stop.first, stop.second, stop.third };
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
    reproc_stop(process, process->stop);
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

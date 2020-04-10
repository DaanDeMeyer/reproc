#include <reproc/reproc.h>

#include "clock.h"
#include "error.h"
#include "handle.h"
#include "init.h"
#include "macro.h"
#include "options.h"
#include "pipe.h"
#include "process.h"
#include "redirect.h"

#include <stdlib.h>

struct reproc_t {
  process_type handle;
  struct {
    pipe_type in;
    pipe_type out;
    pipe_type err;
    pipe_type exit;
  } pipe;
  int status;
  reproc_stop_actions stop;
  int64_t deadline;
};

enum { STATUS_NOT_STARTED = -1, STATUS_IN_PROGRESS = -2, STATUS_IN_CHILD = -3 };

#define SIGOFFSET 128

const int REPROC_SIGKILL = SIGOFFSET + 9;
const int REPROC_SIGTERM = SIGOFFSET + 15;

const int REPROC_INFINITE = -1;
const int REPROC_DEADLINE = -2;

static int setup_input(pipe_type *pipe, const uint8_t *data, size_t size)
{
  if (data == NULL || size == 0) {
    ASSERT(data == NULL);
    ASSERT(size == 0);
    return 0;
  }

  ASSERT(pipe && *pipe != PIPE_INVALID);

  // `reproc_write` only needs the child process stdin pipe to be initialized.
  size_t written = 0;
  int r = -1;

  // Make sure we don't block indefinitely when `input` is bigger than the
  // size of the pipe.
  r = pipe_nonblocking(*pipe, true);
  if (r < 0) {
    return r;
  }

  while (written < size) {
    r = pipe_write(*pipe, data + written, size - written);
    if (r < 0) {
      return r;
    }

    ASSERT(written + (size_t) r <= size);
    written += (size_t) r;
  }

  *pipe = pipe_destroy(*pipe);

  return 0;
}

static int expiry(int timeout, int64_t deadline)
{
  if (timeout == REPROC_INFINITE && deadline == REPROC_INFINITE) {
    return REPROC_INFINITE;
  }

  if (deadline == REPROC_INFINITE) {
    return timeout;
  }

  int64_t n = now();

  if (n >= deadline) {
    return 0;
  }

  // `deadline` exceeds `now` by at most a full `int` so the cast is safe.
  int remaining = (int) (deadline - n);

  if (timeout == REPROC_INFINITE) {
    return remaining;
  }

  return MIN(timeout, remaining);
}

static size_t find_earliest_deadline(reproc_event_source *sources,
                                     size_t num_sources)
{
  ASSERT(sources);
  ASSERT(num_sources > 0);

  size_t earliest = 0;
  int min = REPROC_INFINITE;

  for (size_t i = 0; i < num_sources; i++) {
    reproc_t *process = sources[i].process;

    if (process == NULL) {
      continue;
    }

    int current = expiry(REPROC_INFINITE, process->deadline);

    if (min == REPROC_INFINITE || current < min) {
      earliest = i;
      min = current;
    }
  }

  return earliest;
}

reproc_t *reproc_new(void)
{
  reproc_t *process = malloc(sizeof(reproc_t));
  if (process == NULL) {
    return NULL;
  }

  *process = (reproc_t){ .handle = PROCESS_INVALID,
                         .pipe = { .in = PIPE_INVALID,
                                   .out = PIPE_INVALID,
                                   .err = PIPE_INVALID,
                                   .exit = PIPE_INVALID },
                         .status = STATUS_NOT_STARTED,
                         .deadline = REPROC_INFINITE };

  return process;
}

int reproc_start(reproc_t *process,
                 const char *const *argv,
                 reproc_options options)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status == STATUS_NOT_STARTED);

  struct {
    handle_type in;
    handle_type out;
    handle_type err;
    pipe_type exit;
  } child = { HANDLE_INVALID, HANDLE_INVALID, HANDLE_INVALID, PIPE_INVALID };
  int r = -1;

  r = init();
  if (r < 0) {
    return r; // Make sure we can always call `deinit` in `finish`.
  }

  r = parse_options(&options, argv);
  if (r < 0) {
    goto finish;
  }

  r = redirect_init(&process->pipe.in, &child.in, REPROC_STREAM_IN,
                    options.redirect.in, options.nonblocking, HANDLE_INVALID);
  if (r < 0) {
    goto finish;
  }

  r = redirect_init(&process->pipe.out, &child.out, REPROC_STREAM_OUT,
                    options.redirect.out, options.nonblocking, HANDLE_INVALID);
  if (r < 0) {
    goto finish;
  }

  r = redirect_init(&process->pipe.err, &child.err, REPROC_STREAM_ERR,
                    options.redirect.err, options.nonblocking, child.out);
  if (r < 0) {
    goto finish;
  }

  r = pipe_init(&process->pipe.exit, &child.exit);
  if (r < 0) {
    goto finish;
  }

  r = setup_input(&process->pipe.in, options.input.data, options.input.size);
  if (r < 0) {
    goto finish;
  }

  struct process_options process_options = {
    .environment = options.environment,
    .working_directory = options.working_directory,
    .handle = { .in = child.in,
                .out = child.out,
                .err = child.err,
                .exit = (handle_type) child.exit }
  };

  r = process_start(&process->handle, argv, process_options);
  if (r < 0) {
    goto finish;
  }

  if (r > 0) {
    process->stop = options.stop;

    if (options.deadline != REPROC_INFINITE) {
      process->deadline = now() + options.deadline;
    }
  }

finish:
  // Either an error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way, they can
  // be safely closed.
  redirect_destroy(child.in, options.redirect.in.type);
  redirect_destroy(child.out, options.redirect.out.type);
  redirect_destroy(child.err, options.redirect.err.type);
  pipe_destroy(child.exit);

  if (r < 0) {
    process->handle = process_destroy(process->handle);
    process->pipe.in = pipe_destroy(process->pipe.in);
    process->pipe.out = pipe_destroy(process->pipe.out);
    process->pipe.err = pipe_destroy(process->pipe.err);
    process->pipe.exit = pipe_destroy(process->pipe.exit);
    deinit();
  } else if (r == 0) {
    process->handle = PROCESS_INVALID;
    // `process_start` has already taken care of closing the handles for us.
    process->pipe.in = PIPE_INVALID;
    process->pipe.out = PIPE_INVALID;
    process->pipe.err = PIPE_INVALID;
    process->pipe.exit = PIPE_INVALID;
    process->status = STATUS_IN_CHILD;
  } else {
    process->status = STATUS_IN_PROGRESS;
  }

  return r;
}

enum { PIPES_PER_SOURCE = 4 };

static bool contains_valid_pipe(pipe_event_source *sources, size_t num_sources)
{
  for (size_t i = 0; i < num_sources; i++) {
    if (sources[i].pipe != PIPE_INVALID) {
      return true;
    }
  }

  return false;
}

int reproc_poll(reproc_event_source *sources, size_t num_sources, int timeout)
{
  ASSERT_EINVAL(sources);
  ASSERT_EINVAL(num_sources > 0);

  size_t earliest = find_earliest_deadline(sources, num_sources);
  int64_t deadline = sources[earliest].process == NULL
                         ? REPROC_INFINITE
                         : sources[earliest].process->deadline;

  if (deadline == 0) {
    sources[earliest].events = REPROC_EVENT_DEADLINE;
    return 0;
  }

  int first = expiry(timeout, deadline);
  size_t num_pipes = num_sources * PIPES_PER_SOURCE;
  int r = REPROC_ENOMEM;

  pipe_event_source *pipes = calloc(num_pipes, sizeof(pipe_event_source));
  if (pipes == NULL) {
    return r;
  }

  for (size_t i = 0; i < num_pipes; i++) {
    pipes[i].pipe = PIPE_INVALID;
  }

  for (size_t i = 0; i < num_sources; i++) {
    size_t j = i * PIPES_PER_SOURCE;
    reproc_t *process = sources[i].process;
    int interests = sources[i].interests;

    if (process == NULL) {
      continue;
    }

    bool in = interests & REPROC_EVENT_IN;
    pipes[j + 0].pipe = in ? process->pipe.in : PIPE_INVALID;
    pipes[j + 0].interests = PIPE_EVENT_OUT;

    bool out = interests & REPROC_EVENT_OUT;
    pipes[j + 1].pipe = out ? process->pipe.out : PIPE_INVALID;
    pipes[j + 1].interests = PIPE_EVENT_IN;

    bool err = interests & REPROC_EVENT_ERR;
    pipes[j + 2].pipe = err ? process->pipe.err : PIPE_INVALID;
    pipes[j + 2].interests = PIPE_EVENT_IN;

    bool exit = interests & REPROC_EVENT_EXIT;
    pipes[j + 3].pipe = exit ? process->pipe.exit : PIPE_INVALID;
    pipes[j + 3].interests = PIPE_EVENT_IN;
  }

  if (!contains_valid_pipe(pipes, num_pipes)) {
    r = REPROC_EPIPE;
    goto finish;
  }

  r = pipe_poll(pipes, num_pipes, first);

  if (r == REPROC_ETIMEDOUT) {
    // Differentiate between timeout and deadline expiry. Deadline expiry is an
    // event, timeout is an error.
    sources[earliest].events = first == deadline ? REPROC_EVENT_DEADLINE : 0;
    r = first == deadline ? 0 : REPROC_ETIMEDOUT;
    goto finish;
  }

  if (r < 0) {
    goto finish;
  }

  for (size_t i = 0; i < num_sources; i++) {
    sources[i].events = 0;
  }

  for (size_t i = 0; i < num_pipes; i++) {
    if (pipes[i].pipe == PIPE_INVALID) {
      continue;
    }

    if (pipes[i].events > 0) {
      // Index in a set of pipes determines the process pipe and thus the
      // process event.
      // 0 = stdin pipe => REPROC_EVENT_IN
      // 1 = stdout pipe => REPROC_EVENT_OUT
      // ...
      int event = 1 << (i % PIPES_PER_SOURCE);
      sources[i / PIPES_PER_SOURCE].events |= event;
    }
  }

finish:
  free(pipes);

  return r < 0 ? r : 0;
}

int reproc_read(reproc_t *process,
                REPROC_STREAM stream,
                uint8_t *buffer,
                size_t size)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status != STATUS_IN_CHILD);
  ASSERT_EINVAL(stream == REPROC_STREAM_OUT || stream == REPROC_STREAM_ERR);
  ASSERT_EINVAL(buffer);

  pipe_type *pipe = stream == REPROC_STREAM_OUT ? &process->pipe.out
                                                : &process->pipe.err;
  if (*pipe == PIPE_INVALID) {
    return REPROC_EPIPE;
  }

  int r = pipe_read(*pipe, buffer, size);

  if (r == REPROC_EPIPE) {
    *pipe = pipe_destroy(*pipe);
  }

  return r;
}

int reproc_write(reproc_t *process, const uint8_t *buffer, size_t size)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status != STATUS_IN_CHILD);

  if (buffer == NULL) {
    // Allow `NULL` buffers but only if `size == 0`.
    ASSERT_EINVAL(size == 0);
    return 0;
  }

  if (process->pipe.in == PIPE_INVALID) {
    return REPROC_EPIPE;
  }

  int r = pipe_write(process->pipe.in, buffer, size);

  if (r == REPROC_EPIPE) {
    process->pipe.in = pipe_destroy(process->pipe.in);
  }

  return r;
}

int reproc_close(reproc_t *process, REPROC_STREAM stream)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status != STATUS_IN_CHILD);

  switch (stream) {
    case REPROC_STREAM_IN:
      process->pipe.in = pipe_destroy(process->pipe.in);
      return 0;
    case REPROC_STREAM_OUT:
      process->pipe.out = pipe_destroy(process->pipe.out);
      return 0;
    case REPROC_STREAM_ERR:
      process->pipe.err = pipe_destroy(process->pipe.err);
      return 0;
  }

  return REPROC_EINVAL;
}

int reproc_wait(reproc_t *process, int timeout)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status != STATUS_IN_CHILD);
  ASSERT_EINVAL(process->status != STATUS_NOT_STARTED);

  int r = -1;

  if (process->status >= 0) {
    return process->status;
  }

  if (timeout == REPROC_DEADLINE) {
    // If the deadline has expired, `expiry` returns 0 which means we'll only
    // check if the process is still running.
    timeout = expiry(REPROC_INFINITE, process->deadline);
  }

  ASSERT(process->pipe.exit != PIPE_INVALID);

  pipe_event_source source = { .pipe = process->pipe.exit,
                               .interests = PIPE_EVENT_IN };

  r = pipe_poll(&source, 1, timeout);
  if (r < 0) {
    return r;
  }

  r = process_wait(process->handle);
  if (r < 0) {
    return r;
  }

  process->pipe.exit = pipe_destroy(process->pipe.exit);

  return process->status = r;
}

int reproc_terminate(reproc_t *process)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status != STATUS_IN_CHILD);
  ASSERT_EINVAL(process->status != STATUS_NOT_STARTED);

  if (process->status >= 0) {
    return 0;
  }

  return process_terminate(process->handle);
}

int reproc_kill(reproc_t *process)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status != STATUS_IN_CHILD);
  ASSERT_EINVAL(process->status != STATUS_NOT_STARTED);

  if (process->status >= 0) {
    return 0;
  }

  return process_kill(process->handle);
}

int reproc_stop(reproc_t *process, reproc_stop_actions stop)
{
  ASSERT_EINVAL(process);
  ASSERT_EINVAL(process->status != STATUS_IN_CHILD);
  ASSERT_EINVAL(process->status != STATUS_NOT_STARTED);

  reproc_stop_action actions[] = { stop.first, stop.second, stop.third };
  int r = -1;

  for (size_t i = 0; i < ARRAY_SIZE(actions); i++) {
    r = REPROC_EINVAL; // NOLINT

    switch (actions[i].action) {
      case REPROC_STOP_NOOP:
        r = 0;
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
  ASSERT_RETURN(process, NULL);

  if (process->status == STATUS_IN_PROGRESS) {
    reproc_stop(process, process->stop);
  }

  process_destroy(process->handle);
  pipe_destroy(process->pipe.in);
  pipe_destroy(process->pipe.out);
  pipe_destroy(process->pipe.err);
  pipe_destroy(process->pipe.exit);

  if (process->status != STATUS_NOT_STARTED) {
    deinit();
  }

  free(process);

  return NULL;
}

const char *reproc_strerror(int error)
{
  return error_string(error);
}

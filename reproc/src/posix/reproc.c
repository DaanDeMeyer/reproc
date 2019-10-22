#include <reproc/reproc.h>

#include <macro.h>
#include <posix/fd.h>
#include <posix/pipe.h>
#include <posix/process.h>

#include <assert.h>
#include <stddef.h>

REPROC_ERROR reproc_start(reproc_t *process,
                          const char *const *argv,
                          const char *const *environment,
                          const char *working_directory)
{
  assert(process);
  assert(argv);
  assert(argv[0] != NULL);

  process->running = false;

  // Predeclare every variable so we can use `goto`.

  int child_stdin = 0;
  int child_stdout = 0;
  int child_stderr = 0;

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  const struct pipe_options blocking = { .nonblocking = false };
  const struct pipe_options nonblocking = { .nonblocking = true };

  // Create the pipes used to redirect the child process' stdin/stdout/stderr to
  // the parent process.

  error = pipe_init(&child_stdin, blocking, &process->in, nonblocking);
  if (error) {
    goto cleanup;
  }

  error = pipe_init(&process->out, nonblocking, &child_stdout, blocking);
  if (error) {
    goto cleanup;
  }

  // We poll the output pipes so we put the parent ends of the output pipes in
  // non-blocking mode.

  error = pipe_init(&process->err, nonblocking, &child_stderr, blocking);
  if (error) {
    goto cleanup;
  }

  struct process_options options = { .environment = environment,
                                     .working_directory = working_directory,
                                     .stdin_fd = child_stdin,
                                     .stdout_fd = child_stdout,
                                     .stderr_fd = child_stderr };

  // Fork a child process and call `execve`.
  error = process_create(argv, &options, &process->id);

cleanup:
  // Either an error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way they can
  // be safely closed in the parent process.
  fd_close(&child_stdin);
  fd_close(&child_stdout);
  fd_close(&child_stderr);

  if (error) {
    reproc_destroy(process);
  } else {
    process->running = true;
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
  assert(buffer);
  assert(bytes_read);

  int ready = 0;

  REPROC_ERROR error = pipe_wait(&ready, process->out, process->err);
  if (error) {
    return error;
  }

  error = pipe_read(ready, buffer, size, bytes_read);
  if (error) {
    return error;
  }

  // Indicate which stream was read from if requested by the user.
  if (stream != NULL) {
    *stream = ready == process->out ? REPROC_STREAM_OUT : REPROC_STREAM_ERR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_write(reproc_t *process,
                          const uint8_t *buffer,
                          unsigned int size,
                          unsigned int *bytes_written)
{
  assert(process);
  assert(process->in != 0);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(process->in, buffer, size, bytes_written);
}

void reproc_close(reproc_t *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
    case REPROC_STREAM_IN:
      fd_close(&process->in);
      return;
    case REPROC_STREAM_OUT:
      fd_close(&process->out);
      return;
    case REPROC_STREAM_ERR:
      fd_close(&process->err);
      return;
  }

  assert(false);
}

REPROC_ERROR reproc_wait(reproc_t *process, unsigned int timeout)
{
  assert(process);

  if (!process->running) {
    return REPROC_SUCCESS;
  }

  REPROC_ERROR error = process_wait(process->id, timeout,
                                    &process->exit_status);

  if (error == REPROC_SUCCESS) {
    process->running = false;
  }

  return error;
}

REPROC_ERROR reproc_terminate(reproc_t *process)
{
  assert(process);

  if (!process->running) {
    return REPROC_SUCCESS;
  }

  return process_terminate(process->id);
}

REPROC_ERROR reproc_kill(reproc_t *process)
{
  assert(process);

  if (!process->running) {
    return REPROC_SUCCESS;
  }

  return process_kill(process->id);
}

void reproc_destroy(reproc_t *process)
{
  assert(process);

  fd_close(&process->in);
  fd_close(&process->out);
  fd_close(&process->err);
}

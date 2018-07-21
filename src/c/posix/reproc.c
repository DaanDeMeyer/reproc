#include "reproc/reproc.h"

#include "constants.h"
#include "fork_exec_redirect.h"
#include "pipe.h"
#include "wait.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

REPROC_ERROR reproc_init(reproc_type *reproc)
{
  assert(reproc);

  reproc->id = PID_NULL;

  reproc->parent_stdin = PIPE_NULL;
  reproc->parent_stdout = PIPE_NULL;
  reproc->parent_stderr = PIPE_NULL;
  // We save the exit status because after calling waitpid multiple times on a
  // process that has already exited is unsafe since after the first time the
  // system can reuse the process id for another process.
  reproc->exit_status = EXIT_STATUS_NULL;

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_start(reproc_type *reproc, int argc,
                          const char *const *argv,
                          const char *working_directory)
{
  assert(reproc);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  // Make sure reproc_start is only called once for each reproc_init call
  // (reproc_init sets reproc->id to PID_NULL)
  assert(reproc->id == PID_NULL);

  int child_stdin = PIPE_NULL;
  int child_stdout = PIPE_NULL;
  int child_stderr = PIPE_NULL;

  REPROC_ERROR error = REPROC_SUCCESS;

  error = pipe_init(&child_stdin, &reproc->parent_stdin);
  if (error) { goto cleanup; }
  error = pipe_init(&reproc->parent_stdout, &child_stdout);
  if (error) { goto cleanup; }
  error = pipe_init(&reproc->parent_stderr, &child_stderr);
  if (error) { goto cleanup; }

  error = fork_exec_redirect(argc, argv, working_directory, child_stdin,
                             child_stdout, child_stderr, &reproc->id);

cleanup:
  // An error has ocurred or the child pipe endpoints have been copied to the
  // the stdin/stdout/stderr streams of the child process. Either way they can
  // be safely closed in the parent process
  pipe_close(&child_stdin);
  pipe_close(&child_stdout);
  pipe_close(&child_stderr);

  if (error) {
    reproc_destroy(reproc);
    return error;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_write(reproc_type *reproc, const void *buffer,
                          unsigned int to_write, unsigned int *bytes_written)
{
  assert(reproc);
  assert(reproc->parent_stdin != PIPE_NULL);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(reproc->parent_stdin, buffer, to_write, bytes_written);
}

REPROC_ERROR reproc_close(struct reproc_type *reproc, REPROC_STREAM stream)
{
  assert(reproc);

  switch (stream) {
  case REPROC_STDIN: pipe_close(&reproc->parent_stdin); break;
  case REPROC_STDOUT: pipe_close(&reproc->parent_stdout); break;
  case REPROC_STDERR: pipe_close(&reproc->parent_stderr); break;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_read(reproc_type *reproc, REPROC_STREAM stream,
                         void *buffer, unsigned int size,
                         unsigned int *bytes_read)
{
  assert(reproc);
  assert(stream != REPROC_STDIN); // stream cannot be REPROC_STDIN
  assert(buffer);
  assert(bytes_read);

  switch (stream) {
  case REPROC_STDIN: break;
  case REPROC_STDOUT:
    return pipe_read(reproc->parent_stdout, buffer, size, bytes_read);
  case REPROC_STDERR:
    return pipe_read(reproc->parent_stderr, buffer, size, bytes_read);
  }

  // Unreachable but write anyway to silence warning
  return REPROC_UNKNOWN_ERROR;
}

REPROC_ERROR reproc_wait(reproc_type *reproc, unsigned int milliseconds)
{
  assert(reproc);
  assert(reproc->id != PID_NULL);

  // Don't wait if child process has already exited. We don't use waitpid for
  // this because if we've already waited once after the process has exited the
  // pid of the process might have already been reused by the system.
  if (reproc->exit_status != EXIT_STATUS_NULL) { return REPROC_SUCCESS; }

  if (milliseconds == 0) {
    return wait_no_hang(reproc->id, &reproc->exit_status);
  }

  if (milliseconds == REPROC_INFINITE) {
    return wait_infinite(reproc->id, &reproc->exit_status);
  }

  return wait_timeout(reproc->id, &reproc->exit_status, milliseconds);
}

REPROC_ERROR reproc_terminate(struct reproc_type *reproc,
                              unsigned int milliseconds)
{
  assert(reproc);
  assert(reproc->id != PID_NULL);

  // Check if child process has already exited before sending signal
  REPROC_ERROR error = reproc_wait(reproc, 0);

  // Return if wait succeeds (which means the child process has already exited)
  // or if an error other than a wait timeout occurs during waiting
  if (error != REPROC_WAIT_TIMEOUT) { return error; }

  errno = 0;
  if (kill(reproc->id, SIGTERM) == -1) { return REPROC_UNKNOWN_ERROR; }

  return reproc_wait(reproc, milliseconds);
}

REPROC_ERROR reproc_kill(reproc_type *reproc, unsigned int milliseconds)
{
  assert(reproc);
  assert(reproc->id != PID_NULL);

  // Check if child process has already exited before sending signal
  REPROC_ERROR error = reproc_wait(reproc, 0);

  // Return if wait succeeds (which means the child process has already exited)
  // or if an error other than a wait timeout occurs during waiting
  if (error != REPROC_WAIT_TIMEOUT) { return error; }

  errno = 0;
  if (kill(reproc->id, SIGKILL) == -1) { return REPROC_UNKNOWN_ERROR; }

  return reproc_wait(reproc, milliseconds);
}

REPROC_ERROR reproc_exit_status(reproc_type *reproc, int *exit_status)
{
  assert(reproc);
  assert(exit_status);

  if (reproc->exit_status == EXIT_STATUS_NULL) { return REPROC_STILL_RUNNING; }

  *exit_status = reproc->exit_status;

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_destroy(reproc_type *reproc)
{
  assert(reproc);

  pipe_close(&reproc->parent_stdin);
  pipe_close(&reproc->parent_stdout);
  pipe_close(&reproc->parent_stderr);

  return REPROC_SUCCESS;
}

unsigned int reproc_system_error(void) { return (unsigned int) errno; }

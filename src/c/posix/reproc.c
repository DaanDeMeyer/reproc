#include "reproc/reproc.h"

#include "fork_exec_redirect.h"
#include "pipe.h"
#include "wait.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

void reproc_init(reproc_type *reproc)
{
  assert(reproc);

  // process id 0 is reserved by the system so we can use it as a null value
  reproc->id = 0;
  // File descriptor 0 won't be assigned by pipe() call (its reserved for stdin)
  // so we use it as a null value
  reproc->parent_stdin = 0;
  reproc->parent_stdout = 0;
  reproc->parent_stderr = 0;
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
  assert(reproc->id == 0);

  int child_stdin = 0;
  int child_stdout = 0;
  int child_stderr = 0;

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
  assert(reproc->parent_stdin != 0);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(reproc->parent_stdin, buffer, to_write, bytes_written);
}

void reproc_close(struct reproc_type *reproc, REPROC_STREAM stream)
{
  assert(reproc);

  switch (stream) {
  case REPROC_STDIN: pipe_close(&reproc->parent_stdin); break;
  case REPROC_STDOUT: pipe_close(&reproc->parent_stdout); break;
  case REPROC_STDERR: pipe_close(&reproc->parent_stderr); break;
  }
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

REPROC_ERROR reproc_wait(reproc_type *reproc, unsigned int milliseconds,
                         unsigned int *exit_status)
{
  assert(reproc);
  assert(reproc->id != 0);
  assert(exit_status);

  if (milliseconds == 0) {
    return wait_no_hang(reproc->id, exit_status);
  }

  if (milliseconds == REPROC_INFINITE) {
    return wait_infinite(reproc->id, exit_status);
  }

  return wait_timeout(reproc->id, milliseconds, exit_status);
}

REPROC_ERROR reproc_terminate(struct reproc_type *reproc,
                              unsigned int milliseconds)
{
  assert(reproc);
  assert(reproc->id != 0);

  errno = 0;
  if (kill(reproc->id, SIGTERM) == -1) { return REPROC_UNKNOWN_ERROR; }

  return reproc_wait(reproc, milliseconds, NULL);
}

REPROC_ERROR reproc_kill(reproc_type *reproc, unsigned int milliseconds)
{
  assert(reproc);
  assert(reproc->id != 0);

  errno = 0;
  if (kill(reproc->id, SIGKILL) == -1) { return REPROC_UNKNOWN_ERROR; }

  return reproc_wait(reproc, milliseconds, NULL);
}

void reproc_destroy(reproc_type *reproc)
{
  assert(reproc);

  pipe_close(&reproc->parent_stdin);
  pipe_close(&reproc->parent_stdout);
  pipe_close(&reproc->parent_stderr);
}

unsigned int reproc_system_error(void) { return (unsigned int) errno; }

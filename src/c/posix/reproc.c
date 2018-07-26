#include "reproc/reproc.h"

#include "fork.h"
#include "pipe.h"
#include "wait.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

REPROC_ERROR reproc_start(reproc_type *process, int argc,
                          const char *const *argv,
                          const char *working_directory)
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  // Predeclare every variable so we can use goto

  int child_stdin = 0;
  int child_stdout = 0;
  int child_stderr = 0;

  REPROC_ERROR error = REPROC_SUCCESS;

  error = pipe_init(&child_stdin, &process->parent_stdin);
  if (error) { goto cleanup; }
  error = pipe_init(&process->parent_stdout, &child_stdout);
  if (error) { goto cleanup; }
  error = pipe_init(&process->parent_stderr, &child_stderr);
  if (error) { goto cleanup; }

  error = fork_exec_redirect(argc, argv, working_directory, child_stdin,
                             child_stdout, child_stderr, &process->id);

cleanup:
  // An error has ocurred or the child pipe endpoints have been copied to the
  // the stdin/stdout/stderr streams of the child process. Either way they can
  // be safely closed in the parent process
  pipe_close(&child_stdin);
  pipe_close(&child_stdout);
  pipe_close(&child_stderr);

  if (error) {
    reproc_destroy(process);
    return error;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_write(reproc_type *process, const void *buffer,
                          unsigned int to_write, unsigned int *bytes_written)
{
  assert(process);
  assert(process->parent_stdin != 0);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(process->parent_stdin, buffer, to_write, bytes_written);
}

void reproc_close(struct reproc_type *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
  case REPROC_STDIN: pipe_close(&process->parent_stdin); break;
  case REPROC_STDOUT: pipe_close(&process->parent_stdout); break;
  case REPROC_STDERR: pipe_close(&process->parent_stderr); break;
  }
}

REPROC_ERROR reproc_read(reproc_type *process, REPROC_STREAM stream,
                         void *buffer, unsigned int size,
                         unsigned int *bytes_read)
{
  assert(process);
  assert(stream != REPROC_STDIN);
  assert(buffer);
  assert(bytes_read);

  switch (stream) {
  case REPROC_STDIN: break;
  case REPROC_STDOUT:
    return pipe_read(process->parent_stdout, buffer, size, bytes_read);
  case REPROC_STDERR:
    return pipe_read(process->parent_stderr, buffer, size, bytes_read);
  }

  // Only reachable when compiled without asserts
  return REPROC_UNKNOWN_ERROR;
}

REPROC_ERROR reproc_wait(reproc_type *process, unsigned int milliseconds,
                         unsigned int *exit_status)
{
  assert(process);
  assert(process->id != 0);

  if (milliseconds == 0) { return wait_no_hang(process->id, exit_status); }

  if (milliseconds == REPROC_INFINITE) {
    return wait_infinite(process->id, exit_status);
  }

  return wait_timeout(process->id, milliseconds, exit_status);
}

REPROC_ERROR reproc_terminate(struct reproc_type *process,
                              unsigned int milliseconds)
{
  assert(process);
  assert(process->id != 0);

  errno = 0;
  if (kill(process->id, SIGTERM) == -1) { return REPROC_UNKNOWN_ERROR; }

  return reproc_wait(process, milliseconds, NULL);
}

REPROC_ERROR reproc_kill(reproc_type *process, unsigned int milliseconds)
{
  assert(process);
  assert(process->id != 0);

  errno = 0;
  if (kill(process->id, SIGKILL) == -1) { return REPROC_UNKNOWN_ERROR; }

  return reproc_wait(process, milliseconds, NULL);
}

void reproc_destroy(reproc_type *process)
{
  assert(process);

  pipe_close(&process->parent_stdin);
  pipe_close(&process->parent_stdout);
  pipe_close(&process->parent_stderr);
}

unsigned int reproc_system_error(void) { return (unsigned int) errno; }

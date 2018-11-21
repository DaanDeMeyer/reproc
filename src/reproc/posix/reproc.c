#include <reproc/reproc.h>

#include "fd.h"
#include "pipe.h"
#include "process.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

// Makeshift C lambda (receives its arguments from process_create).
static int exec_process(const void *data)
{
  const char *const *argv = data;

  // Replace the forked process with the process specified in argv's first
  // element. The cast is safe since execvp doesn't actually change the contents
  // of argv.
  if (execvp(argv[0], (char **) argv) == -1) { return errno; }

  return 0;
}

static REPROC_ERROR exec_map_error(int error)
{
  switch (error) {
  case E2BIG: return REPROC_ARGS_TOO_LONG;
  case EACCES: return REPROC_PERMISSION_DENIED;
  case ELOOP: return REPROC_SYMLINK_LOOP;
  case EMFILE: return REPROC_PROCESS_LIMIT_REACHED;
  case ENAMETOOLONG: return REPROC_NAME_TOO_LONG;
  case ENOENT: return REPROC_FILE_NOT_FOUND;
  case ENOTDIR: return REPROC_FILE_NOT_FOUND;
  case ENOEXEC: return REPROC_NOT_EXECUTABLE;
  case ENOMEM: return REPROC_NOT_ENOUGH_MEMORY;
  case EPERM: return REPROC_PERMISSION_DENIED;
  default: return REPROC_UNKNOWN_ERROR;
  }
}

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

  // Predeclare every variable so we can use goto.

  int child_stdin = 0;
  int child_stdout = 0;
  int child_stderr = 0;

  REPROC_ERROR error = REPROC_SUCCESS;

  error = pipe_init(&child_stdin, &process->in);
  if (error) { goto cleanup; }
  error = pipe_init(&process->out, &child_stdout);
  if (error) { goto cleanup; }
  error = pipe_init(&process->err, &child_stderr);
  if (error) { goto cleanup; }

  struct process_options options = {
    .working_directory = working_directory,
    .stdin_fd = child_stdin,
    .stdout_fd = child_stdout,
    .stderr_fd = child_stderr,
    // We put the child process in its own process group which is needed by
    // wait_timeout in process.c (see wait_timeout for extra information).
    .process_group = 0,
    // Don't return early to make sure we receive errors reported by execve.
    .return_early = false
  };

  // Fork a child process and call exec.
  error = process_create(exec_process, argv, &options, &process->id);
  if (error == REPROC_UNKNOWN_ERROR) { error = exec_map_error(errno); }

cleanup:
  // An error has ocurred or the child pipe endpoints have been copied to the
  // stdin/stdout/stderr streams of the child process. Either way they can be
  // safely closed in the parent process.
  fd_close(&child_stdin);
  fd_close(&child_stdout);
  fd_close(&child_stderr);

  if (error) { reproc_destroy(process); }

  return error;
}

REPROC_ERROR reproc_read(reproc_type *process, REPROC_STREAM stream,
                         void *buffer, unsigned int size,
                         unsigned int *bytes_read)
{
  assert(process);
  assert(stream != REPROC_IN);
  assert(buffer);
  assert(bytes_read);

  switch (stream) {
  case REPROC_IN: break;
  case REPROC_OUT: return pipe_read(process->out, buffer, size, bytes_read);
  case REPROC_ERR: return pipe_read(process->err, buffer, size, bytes_read);
  }

  assert(0);
  return REPROC_UNKNOWN_ERROR;
}

REPROC_ERROR reproc_write(reproc_type *process, const void *buffer,
                          unsigned int to_write, unsigned int *bytes_written)
{
  assert(process);
  assert(process->in != 0);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(process->in, buffer, to_write, bytes_written);
}

void reproc_close(reproc_type *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
  case REPROC_IN: fd_close(&process->in); return;
  case REPROC_OUT: fd_close(&process->out); return;
  case REPROC_ERR: fd_close(&process->err); return;
  }

  assert(0);
}

REPROC_ERROR reproc_wait(reproc_type *process, unsigned int timeout,
                         unsigned int *exit_status)
{
  assert(process);

  return process_wait(process->id, timeout, exit_status);
}

REPROC_ERROR reproc_terminate(reproc_type *process)
{
  assert(process);

  return process_terminate(process->id);
}

REPROC_ERROR reproc_kill(reproc_type *process)
{
  assert(process);

  return process_kill(process->id);
}

void reproc_destroy(reproc_type *process)
{
  assert(process);

  fd_close(&process->in);
  fd_close(&process->out);
  fd_close(&process->err);
}

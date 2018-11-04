#include "reproc/reproc.h"

#include "fd.h"
#include "pipe.h"
#include "process.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

static void reproc_destroy(reproc_type *process)
{
  assert(process);

  fd_close(&process->parent_stdin);
  fd_close(&process->parent_stdout);
  fd_close(&process->parent_stderr);
}

// Makeshift C lambda (receives its arguments from process_create).
static int exec_process(const void *data)
{
  const char *const *argv = data;

  // Replace forked process with the process specified in argv.
  // Safe cast (execvp doesn't actually change the contents of argv).
  if (execvp(argv[0], (char **) argv) == -1) { return errno; }

  return 0;
}

static REPROC_ERROR exec_map_error(int error)
{
  switch(error) {
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

  error = pipe_init(&child_stdin, &process->parent_stdin);
  if (error) { goto cleanup; }
  error = pipe_init(&process->parent_stdout, &child_stdout);
  if (error) { goto cleanup; }
  error = pipe_init(&process->parent_stderr, &child_stderr);
  if (error) { goto cleanup; }

  struct process_options options = {
    .working_directory = working_directory,
    .stdin_fd = child_stdin,
    .stdout_fd = child_stdout,
    .stderr_fd = child_stderr,
    // We put the child process in its own process group which is needed by
    // reproc_stop (see reproc_stop for extra information).
    .process_group = 0,
    // Don't return early to make sure we receive errors reported by execve.
    .return_early = false
  };

  // Fork a child process and call exec.
  error = process_create(exec_process, argv, &options, &process->id);
  if (error == REPROC_UNKNOWN_ERROR) { error = exec_map_error(errno); }

cleanup:
  // An error has ocurred or the child pipe endpoints have been copied to the
  // the stdin/stdout/stderr streams of the child process. Either way they can
  // be safely closed in the parent process.
  fd_close(&child_stdin);
  fd_close(&child_stdout);
  fd_close(&child_stderr);

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

void reproc_close(reproc_type *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
  case REPROC_IN: fd_close(&process->parent_stdin); break;
  case REPROC_OUT: fd_close(&process->parent_stdout); break;
  case REPROC_ERR: fd_close(&process->parent_stderr); break;
  default: assert(0); // Reaching default case indicates logic error.
  }
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
  case REPROC_OUT:
    return pipe_read(process->parent_stdout, buffer, size, bytes_read);
  case REPROC_ERR:
    return pipe_read(process->parent_stderr, buffer, size, bytes_read);
  default: assert(0); // Reaching default case indicates logic error.
  }

  // Only reachable when compiled without asserts.
  return REPROC_UNKNOWN_ERROR;
}

REPROC_ERROR reproc_stop(reproc_type *process, int cleanup_flags,
                         unsigned int t1, unsigned int t2, unsigned int t3,
                         unsigned int *exit_status)
{
  assert(process);

  // We don't set error to REPROC_SUCCESS so we can check if wait/terminate/kill
  // succeeded (in which case error is set to REPROC_SUCCESS).
  REPROC_ERROR error = REPROC_WAIT_TIMEOUT;

  unsigned int timeout[3] = { t1, t2, t3 };
  // Keeps track of the first unassigned timeout.
  unsigned int i = 0;

  if (cleanup_flags & REPROC_WAIT) {
    error = process_wait(process->id, timeout[i], exit_status);
    i++;
  }

  if (error != REPROC_WAIT_TIMEOUT) { goto cleanup; }

  if (cleanup_flags & REPROC_TERMINATE) {
    error = process_terminate(process->id, timeout[i], exit_status);
    i++;
  }

  if (error != REPROC_WAIT_TIMEOUT) { goto cleanup; }

  if (cleanup_flags & REPROC_KILL) {
    error = process_kill(process->id, timeout[i], exit_status);
    i++;
  }

cleanup:
  reproc_destroy(process);

  return error;
}

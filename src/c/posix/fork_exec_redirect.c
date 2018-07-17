#include "fork_exec_redirect.h"

#include "constants.h"
#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

REPROC_ERROR fork_exec_redirect(int argc, const char *argv[],
                                     const char *working_directory,
                                     int stdin_pipe, int stdout_pipe,
                                     int stderr_pipe, pid_t *pid)
{
  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  assert(stdin_pipe >= 0);
  assert(stdout_pipe >= 0);
  assert(stderr_pipe >= 0);
  assert(pid);

  REPROC_ERROR error = REPROC_SUCCESS;

  // We create an error pipe to receive pre-exec and exec errors from the child
  // process. See this answer https://stackoverflow.com/a/1586277 for more
  // information
  int error_pipe_read = PIPE_NULL;
  int error_pipe_write = PIPE_NULL;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) { return error; }

  errno = 0;
  *pid = fork();

  if (*pid == -1) {
    pipe_close(&error_pipe_read);
    pipe_close(&error_pipe_write);

    switch (errno) {
    case EAGAIN: return REPROC_PROCESS_LIMIT_REACHED;
    case ENOMEM: return REPROC_MEMORY_ERROR;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  if (*pid == 0) {
    // In child process (Since we're in the child process we can exit on error)
    // Why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    errno = 0;

    // We put the child process in its own process group which is needed by
    // reproc_wait. Normally there might be a race condition if the parent
    // process waits for the child process before the child process puts itself
    // in its own process group (with setpgid) but this is avoided because we
    // always read from the error pipe in the parent process after forking. When
    // read returns the child process will either have errored out (and waiting
    // won't be valid) or will have executed execvp (and as a result setpgid as
    // well)
    if (setpgid(0, 0) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    if (working_directory && chdir(working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // Redirect stdin, stdout and stderr
    // _exit ensures open file descriptors (pipes) are closed
    if (dup2(stdin_pipe, STDIN_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (dup2(stdout_pipe, STDOUT_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (dup2(stderr_pipe, STDERR_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    int max_fd = (int) sysconf(_SC_OPEN_MAX);
    for (int i = 3; i < max_fd; i++) {
      // We might still need the error pipe if execve fails so we don't close
      // it. The error pipe is created with FD_CLOEXEC which results in it being
      // closed automatically on exec which means we don't have to manually
      // close it for the case that exec succeeds
      if (i == error_pipe_write) { continue; }
      close(i);
    }
    // Ignore file descriptor close errors since we try them all and close sets
    // errno when an invalid file descriptor is passed
    errno = 0;

    // Replace forked child with process we want to run
    // Safe cast (execvp doesn't actually change the contents of argv)
    execvp(argv[0], (char **) argv);

    write(error_pipe_write, &errno, sizeof(errno));

    // Exit if execvp fails
    _exit(errno);
  }

  // Close write end on parent side so read will read 0 if it is closed on the
  // child side as well
  pipe_close(&error_pipe_write);

  int exec_error = 0;
  unsigned int bytes_read = 0;
  error = pipe_read(error_pipe_read, &exec_error, sizeof(exec_error),
                    &bytes_read);
  pipe_close(&error_pipe_read);

  switch (error) {
  case REPROC_SUCCESS: break;
  // REPROC_STREAM_CLOSED indicates the execve was succesful (because
  // FD_CLOEXEC kicked in and closed the error_pipe_write fd in the child
  // process)
  case REPROC_STREAM_CLOSED: break;
  // Conjecture: The chance of REPROC_INTERRUPTED occurring here is really
  // low so we return REPROC_UNKNOWN_ERROR to reduce the error signature of
  // the function.
  case REPROC_INTERRUPTED: return REPROC_UNKNOWN_ERROR;
  default: return error;
  }

  // If an error was written to the error pipe check that a full integer was
  // actually written. We don't expect a partial write to happen but if it ever
  // happens this should make it easier to discover.
  if (error == REPROC_SUCCESS && bytes_read != sizeof(exec_error)) {
    return REPROC_UNKNOWN_ERROR;
  }

  // If read does not return 0 an error will have occurred in the child process
  // before or during execve (or an error with read itself (less likely))
  if (exec_error != 0) {
    // Allow retrieving child process errors with reproc_system_error
    errno = exec_error;

    switch (exec_error) {
    case EACCES: return REPROC_PERMISSION_DENIED;
    case EPERM: return REPROC_PERMISSION_DENIED;
    case ELOOP: return REPROC_SYMLINK_LOOP;
    case ENOMEM: return REPROC_MEMORY_ERROR;
    case ENOENT: return REPROC_FILE_NOT_FOUND;
    case ENOTDIR: return REPROC_FILE_NOT_FOUND;
    case EMFILE: return REPROC_PIPE_LIMIT_REACHED;
    case ENAMETOOLONG: return REPROC_NAME_TOO_LONG;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  return REPROC_SUCCESS;
}

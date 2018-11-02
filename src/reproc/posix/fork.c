#include "fork.h"

#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

REPROC_ERROR fork_action(int (*action)(const void *), const void *data,
                         struct fork_options *options, pid_t *pid)
{
  assert(options->stdin_fd >= 0);
  assert(options->stdout_fd >= 0);
  assert(options->stderr_fd >= 0);
  assert(pid);

  // Predeclare variables so we can use goto.
  REPROC_ERROR error = REPROC_SUCCESS;
  pid_t child_pid = 0;
  int child_error = 0;
  unsigned int bytes_read = 0;

  // We create an error pipe to receive errors from the child process. See this
  // answer https://stackoverflow.com/a/1586277 for more information.
  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) { goto cleanup; }

  errno = 0;
  child_pid = fork();

  if (child_pid == -1) {
    switch (errno) {
    case EAGAIN: error = REPROC_PROCESS_LIMIT_REACHED; break;
    case ENOMEM: error = REPROC_NOT_ENOUGH_MEMORY; break;
    default: error = REPROC_UNKNOWN_ERROR; break;
    }

    goto cleanup;
  }

  if (child_pid == 0) {
    // Child process code. Since we're in the child process we can exit on
    // error. Why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    errno = 0;

    /* Normally there might be a race condition if the parent process waits for
    the child process before the child process puts itself in its own process
    group (with setpgid) but this is avoided because we always read from the
    error pipe in the parent process after forking. When read returns the child
    process will either have errored out (and waiting won't be valid) or will
    have executed _exit or execvp (and as a result setpgid as well). */
    if (setpgid(0, options->process_group) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    if (options->working_directory && chdir(options->working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // Redirect stdin, stdout and stderr if required.
    // _exit ensures open file descriptors (pipes) are closed.

    if (options->stdin_fd && dup2(options->stdin_fd, STDIN_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (options->stdout_fd && dup2(options->stdout_fd, STDOUT_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (options->stderr_fd && dup2(options->stderr_fd, STDERR_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // Close open file descriptors in the child process.

    int max_fd = (int) sysconf(_SC_OPEN_MAX);
    for (int i = 3; i < max_fd; i++) {
      // We might still need the error pipe so we don't close it. The error pipe
      // is created with FD_CLOEXEC which results in it being closed
      // automatically on exec and _exit so we don't have to manually close it.
      if (i == error_pipe_write) { continue; }
      close(i);
    }
    // Ignore file descriptor close errors since we try to close all of them and
    // close sets errno when an invalid file descriptor is passed.
    errno = 0;

    // Close error pipe early if the for is a timeout fork so the calling
    // process doesn't block waiting for the entire timeout to expire.
    if (options->is_timeout_fork) { pipe_close(&error_pipe_write); }

    // Finally, call passed makeshift lambda.
    errno = action(data);

    if (!options->is_timeout_fork) {
      write(error_pipe_write, &errno, sizeof(errno));
    }

    _exit(errno);
  }

  // Close write end on parent side so read will read 0 if it is closed on the
  // child side as well.
  pipe_close(&error_pipe_write);

  error = pipe_read(error_pipe_read, &child_error, sizeof(child_error),
                    &bytes_read);
  pipe_close(&error_pipe_read);

  switch (error) {
  case REPROC_SUCCESS: break;
  // REPROC_STREAM_CLOSED indicates success because the pipe was closed without
  // an error being written to it.
  case REPROC_STREAM_CLOSED: break;
  // Conjecture: The chance of REPROC_INTERRUPTED occurring here is really
  // low so we return REPROC_UNKNOWN_ERROR to reduce the error signature of
  // the function.
  case REPROC_INTERRUPTED: error = REPROC_UNKNOWN_ERROR; goto cleanup;
  default: goto cleanup;
  }

  // If an error was written to the error pipe check that a full integer was
  // actually written. We don't expect a partial write to happen but if it ever
  // happens this should make it easier to discover.
  if (error == REPROC_SUCCESS && bytes_read != sizeof(child_error)) {
    error = REPROC_UNKNOWN_ERROR;
    goto cleanup;
  }

  // If read does not return 0 an error will have occurred in the child process
  // (or with read itself (less likely)).
  if (child_error != 0) {
    // Allow retrieving child process errors with reproc_system_error.
    errno = child_error;

    switch (child_error) {
    case EACCES: error = REPROC_PERMISSION_DENIED; break;
    case EPERM: error = REPROC_PERMISSION_DENIED; break;
    case ELOOP: error = REPROC_SYMLINK_LOOP; break;
    case ENOMEM: error = REPROC_NOT_ENOUGH_MEMORY; break;
    case ENOENT: error = REPROC_FILE_NOT_FOUND; break;
    case ENOTDIR: error = REPROC_FILE_NOT_FOUND; break;
    case EMFILE: error = REPROC_PIPE_LIMIT_REACHED; break;
    case ENAMETOOLONG: error = REPROC_NAME_TOO_LONG; break;
    default: error = REPROC_UNKNOWN_ERROR; break;
    }

    goto cleanup;
  }

cleanup:
  pipe_close(&error_pipe_read);
  pipe_close(&error_pipe_write);

  // REPROC_STREAM_CLOSED is not an error in this scenario (see above).
  if (error != REPROC_SUCCESS && error != REPROC_STREAM_CLOSED &&
      child_pid > 0) {
    // Make sure fork doesn't become a zombie process if an error occurred (and
    // the child process was actually started (child_pid > 0)).
    if (waitpid(child_pid, NULL, 0) == -1) { return REPROC_UNKNOWN_ERROR; }
    return error;
  }

  *pid = child_pid;
  return REPROC_SUCCESS;
}

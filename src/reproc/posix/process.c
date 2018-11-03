#include "process.h"

#include "fd.h"
#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

REPROC_ERROR process_create(int (*action)(const void *), const void *data,
                            struct process_options *options, pid_t *pid)
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

    // Closing the error pipe write end will unblock the pipe_read call in the
    // parent process which allows it to continue executing.
    if (options->return_early) { fd_close(&error_pipe_write); }

    // Finally, call passed makeshift lambda.
    errno = 0;
    errno = action(data);

    // If we didn't return early the error pipe write end is still open and we
    // can use it to report an optional error from action.
    if (!options->return_early) {
      write(error_pipe_write, &errno, sizeof(errno));
    }

    _exit(errno);
  }

  // Close write end on parent side so read will read 0 if it is closed on the
  // child side as well.
  fd_close(&error_pipe_write);

  // Blocks until an error is reported from the child process or the write end
  // of the pipe in the child process is closed.
  error = pipe_read(error_pipe_read, &child_error, sizeof(child_error),
                    &bytes_read);
  fd_close(&error_pipe_read);

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
    case EINTR: error = REPROC_INTERRUPTED; break;
    default: error = REPROC_UNKNOWN_ERROR; break;
    }

    goto cleanup;
  }

cleanup:
  fd_close(&error_pipe_read);
  fd_close(&error_pipe_write);

  // REPROC_STREAM_CLOSED is not an error in this scenario (see above).
  if (error != REPROC_SUCCESS && error != REPROC_STREAM_CLOSED &&
      child_pid > 0) {
    // Make sure the child process doesn't become a zombie process if an error
    // occurred (and the child process was actually started (child_pid > 0)).
    if (waitpid(child_pid, NULL, 0) == -1) { return REPROC_UNKNOWN_ERROR; }
    return error;
  }

  *pid = child_pid;
  return REPROC_SUCCESS;
}

static unsigned int parse_exit_status(int status)
{
  // WEXITSTATUS returns a value between [0,256) so casting to unsigned int is
  // safe.

  if (WIFEXITED(status)) { return (unsigned int) WEXITSTATUS(status); }

  assert(WIFSIGNALED(status));

  return (unsigned int) WTERMSIG(status);
}

static REPROC_ERROR wait_no_hang(pid_t pid, unsigned int *exit_status)
{
  int status = 0;
  errno = 0;
  // Adding WNOHANG makes waitpid only check and return immediately.
  pid_t wait_result = waitpid(pid, &status, WNOHANG);

  if (wait_result == 0) { return REPROC_WAIT_TIMEOUT; }
  if (wait_result == -1) {
    // Ignore EINTR, it shouldn't happen when using WNOHANG.
    return REPROC_UNKNOWN_ERROR;
  }

  if (exit_status) { *exit_status = parse_exit_status(status); }

  return REPROC_SUCCESS;
}

static REPROC_ERROR wait_infinite(pid_t pid, unsigned int *exit_status)
{
  int status = 0;
  errno = 0;
  if (waitpid(pid, &status, 0) == -1) {
    switch (errno) {
    case EINTR: return REPROC_INTERRUPTED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  if (exit_status) { *exit_status = parse_exit_status(status); }

  return REPROC_SUCCESS;
}

// Makeshift C lambda passed to process_create.
static int timeout_process(const void *data)
{
  unsigned int milliseconds = *((const unsigned int *) data);

  struct timeval tv;
  tv.tv_sec = milliseconds / 1000;           // ms -> s
  tv.tv_usec = (milliseconds % 1000) * 1000; // leftover ms -> us

  // Select with no file descriptors can be used as a makeshift sleep function
  // (that can still be interrupted).
  errno = 0;
  if (select(0, NULL, NULL, NULL, &tv) == -1) { return errno; }

  return 0;
}

// See Design section in README.md for an explanation of how this works.
static REPROC_ERROR wait_timeout(pid_t pid, unsigned int timeout,
                                 unsigned int *exit_status)
{
  assert(timeout > 0);

  REPROC_ERROR error = REPROC_SUCCESS;

  // Check if process has already exited before starting (expensive) timeout
  // process. Return if wait succeeds or error (that isn't a timeout) occurs.
  error = wait_no_hang(pid, exit_status);
  if (error != REPROC_WAIT_TIMEOUT) { return error; }

  struct process_options options = {
    // waitpid supports waiting for the first process that exits in a process
    // group. To take advantage of this we put the timeout process in the same
    // process group as the process we're waiting for.
    .process_group = pid,
    // Return early so process_create doesn't block until the timeout has
    // expired.
    .return_early = true
  };

  pid_t timeout_pid = 0;
  error = process_create(timeout_process, &timeout, &options, &timeout_pid);
  if (error) { return error; }

  // -reproc->pid waits for all processes in the reproc->pid process group
  // which in this case will be the process we want to wait for and the timeout
  // process. waitpid will return the process id of whichever process exits
  // first.
  int status = 0;
  errno = 0;
  pid_t exit_pid = waitpid(-pid, &status, 0);

  // If the timeout process exits first the timeout will have been exceeded.
  if (exit_pid == timeout_pid) { return REPROC_WAIT_TIMEOUT; }

  // If the child process exits first we make sure the timeout process is
  // cleaned up correctly by sending a SIGTERM signal and waiting for it.
  errno = 0;
  if (kill(timeout_pid, SIGTERM) == -1) { return REPROC_UNKNOWN_ERROR; }

  errno = 0;
  if (waitpid(timeout_pid, NULL, 0) == -1) {
    switch (errno) {
    case EINTR: return REPROC_INTERRUPTED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  // After cleaning up the timeout process we can check if an error occurred
  // while waiting for the timeout process/child process.
  if (exit_pid == -1) {
    switch (errno) {
    case EINTR: return REPROC_INTERRUPTED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  if (exit_status) { *exit_status = parse_exit_status(status); }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_wait(pid_t pid, unsigned int timeout,
                          unsigned int *exit_status)
{
  if (timeout == 0) { return wait_no_hang(pid, exit_status); }

  if (timeout == 0xFFFFFFFF) { return wait_infinite(pid, exit_status); }

  return wait_timeout(pid, timeout, exit_status);
}

REPROC_ERROR process_terminate(pid_t pid, unsigned int timeout,
                               unsigned int *exit_status)
{
  errno = 0;
  if (kill(pid, SIGTERM) == -1) { return REPROC_UNKNOWN_ERROR; }

  return process_wait(pid, timeout, exit_status);
}

REPROC_ERROR process_kill(pid_t pid, unsigned int timeout,
                          unsigned int *exit_status)
{
  errno = 0;
  if (kill(pid, SIGKILL) == -1) { return REPROC_UNKNOWN_ERROR; }

  return process_wait(pid, timeout, exit_status);
}

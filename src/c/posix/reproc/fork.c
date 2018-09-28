#include "reproc/fork.h"

#include "reproc/pipe.h"

#include <assert.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

REPROC_ERROR fork_exec_redirect(int argc, const char *const *argv,
                                const char *working_directory, int stdin_pipe,
                                int stdout_pipe, int stderr_pipe, pid_t *pid)
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

  // Predeclare variables so we can use goto.
  REPROC_ERROR error = REPROC_SUCCESS;
  pid_t exec_pid = 0;
  int exec_error = 0;
  unsigned int bytes_read = 0;

  // We create an error pipe to receive pre-exec and exec errors from the child
  // process. See this answer https://stackoverflow.com/a/1586277 for more
  // information.
  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) { goto cleanup; }

  errno = 0;
  exec_pid = fork();

  if (exec_pid == -1) {
    switch (errno) {
    case EAGAIN: error = REPROC_PROCESS_LIMIT_REACHED; break;
    case ENOMEM: error = REPROC_NOT_ENOUGH_MEMORY; break;
    default: error = REPROC_UNKNOWN_ERROR; break;
    }

    goto cleanup;
  }

  if (exec_pid == 0) {
    // Child process code. Since we're in the child process we can exit on
    // error. Why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    errno = 0;

    // We put the child process in its own process group which is needed by
    // reproc_stop. Normally there might be a race condition if the parent
    // process waits for the child process before the child process puts itself
    // in its own process group (with setpgid) but this is avoided because we
    // always read from the error pipe in the parent process after forking. When
    // read returns the child process will either have errored out (and waiting
    // won't be valid) or will have executed execvp (and as a result setpgid as
    // well).
    if (setpgid(0, 0) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    if (working_directory && chdir(working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // Redirect stdin, stdout and stderr.
    // _exit ensures open file descriptors (pipes) are closed.
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
      // close it for the case that exec succeeds.
      if (i == error_pipe_write) { continue; }
      close(i);
    }
    // Ignore file descriptor close errors since we try to close all of them and
    // close sets errno when an invalid file descriptor is passed.
    errno = 0;

    // Replace forked child with process we want to run.
    // Safe cast (execvp doesn't actually change the contents of argv).
    execvp(argv[0], (char **) argv);

    write(error_pipe_write, &errno, sizeof(errno));

    // Exit if execvp fails.
    _exit(errno);
  }

  // Close write end on parent side so read will read 0 if it is closed on the
  // child side as well.
  pipe_close(&error_pipe_write);

  error = pipe_read(error_pipe_read, &exec_error, sizeof(exec_error),
                    &bytes_read);
  pipe_close(&error_pipe_read);

  switch (error) {
  case REPROC_SUCCESS: break;
  // REPROC_STREAM_CLOSED indicates the execve was succesful (because
  // FD_CLOEXEC kicked in and closed the error_pipe_write fd in the child
  // process).
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
  if (error == REPROC_SUCCESS && bytes_read != sizeof(exec_error)) {
    error = REPROC_UNKNOWN_ERROR;
    goto cleanup;
  }

  // If read does not return 0 an error will have occurred in the child process
  // before or during execve (or an error with read itself (less likely)).
  if (exec_error != 0) {
    // Allow retrieving child process errors with reproc_system_error.
    errno = exec_error;

    switch (exec_error) {
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
      exec_pid > 0) {
    // Make sure fork doesn't become a zombie process if error occurred.
    if (waitpid(exec_pid, NULL, 0) == -1) { return REPROC_UNKNOWN_ERROR; }
    return error;
  }

  *pid = exec_pid;
  return REPROC_SUCCESS;
}

// This code is the same as above except for the code that gets executed inside
// the fork. If C had closures this duplication wouldn't be necessary. Comments
// outside the fork are also removed here so check the above function for more
// information about the logic.
REPROC_ERROR fork_timeout(unsigned int milliseconds, pid_t process_group,
                          pid_t *pid)
{
  assert(milliseconds > 0);
  assert(pid);

  REPROC_ERROR error = REPROC_SUCCESS;
  pid_t timeout_pid = 0;
  int timeout_error = 0;
  unsigned int bytes_read = 0;

  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) { goto cleanup; }

  errno = 0;
  timeout_pid = fork();

  if (timeout_pid == -1) {
    switch (errno) {
    case EAGAIN: error = REPROC_PROCESS_LIMIT_REACHED; break;
    case ENOMEM: error = REPROC_NOT_ENOUGH_MEMORY; break;
    default: error = REPROC_UNKNOWN_ERROR; break;
    }

    goto cleanup;
  }

  if (timeout_pid == 0) {
    pipe_close(&error_pipe_read);

    errno = 0;
    if (setpgid(0, process_group) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;           // ms -> s
    tv.tv_usec = (milliseconds % 1000) * 1000; // leftover ms -> us

    // we can't check for errors in select without waiting for the timeout to
    // expire so we close the error pipe before calling select.
    pipe_close(&error_pipe_write);

    // Select with no file descriptors can be used as a makeshift sleep function
    // (that can still be interrupted).
    errno = 0;
    if (select(0, NULL, NULL, NULL, &tv) == -1) { _exit(errno); }

    _exit(0);
  }

  pipe_close(&error_pipe_write);

  error = pipe_read(error_pipe_read, &timeout_error, sizeof(timeout_error),
                    &bytes_read);
  pipe_close(&error_pipe_read);

  switch (error) {
  case REPROC_SUCCESS: break;
  case REPROC_STREAM_CLOSED: break;
  case REPROC_INTERRUPTED: error = REPROC_UNKNOWN_ERROR; goto cleanup;
  default: goto cleanup;
  }

  if (error == REPROC_SUCCESS && bytes_read != sizeof(timeout_error)) {
    error = REPROC_UNKNOWN_ERROR;
    goto cleanup;
  }

  if (timeout_error != 0) {
    errno = timeout_error;

    switch (timeout_error) {
    case EINTR: error = REPROC_INTERRUPTED; break;
    case ENOMEM: error = REPROC_NOT_ENOUGH_MEMORY; break;
    default: error = REPROC_UNKNOWN_ERROR; break;
    }

    goto cleanup;
  }

cleanup:
  pipe_close(&error_pipe_read);
  pipe_close(&error_pipe_write);

  if (error != REPROC_SUCCESS && error != REPROC_STREAM_CLOSED &&
      timeout_pid > 0) {
    if (waitpid(timeout_pid, NULL, 0) == -1) { return REPROC_UNKNOWN_ERROR; }
    return error;
  }

  *pid = timeout_pid;
  return REPROC_SUCCESS;
}

#include "wait.h"

#include "constants.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static int parse_exit_status(int status)
{
  if (WIFEXITED(status)) { return WEXITSTATUS(status); }

  assert(WIFSIGNALED(status));
  return WTERMSIG(status);
}

REPROC_ERROR wait_no_hang(pid_t pid, int *exit_status)
{
  assert(exit_status);

  int status = EXIT_STATUS_NULL;
  errno = 0;
  // Adding WNOHANG makes waitpid only check and return immediately
  pid_t wait_result = waitpid(pid, &status, WNOHANG);

  if (wait_result == 0) { return REPROC_WAIT_TIMEOUT; }
  if (wait_result == -1) {
    // Ignore EINTR, it shouldn't happen when using WNOHANG
    return REPROC_UNKNOWN_ERROR;
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}

REPROC_ERROR wait_infinite(pid_t pid, int *exit_status)
{
  assert(exit_status);

  int status = EXIT_STATUS_NULL;
  errno = 0;
  if (waitpid(pid, &status, 0) == -1) {
    switch (errno) {
    case EINTR: return REPROC_INTERRUPTED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}

// See Design section in README.md for an explanation of how this works
REPROC_ERROR wait_timeout(pid_t pid, int *exit_status,
                          unsigned int milliseconds)
{
  assert(exit_status);
  assert(milliseconds > 0);

  errno = 0;
  pid_t timeout_pid = fork();

  if (timeout_pid == -1) {
    switch (errno) {
    case EAGAIN: return REPROC_PROCESS_LIMIT_REACHED;
    case ENOMEM: return REPROC_MEMORY_ERROR;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  // Process group of child is set in both parent and child to avoid race
  // conditions

  if (timeout_pid == 0) {
    errno = 0;
    if (setpgid(0, pid) == -1) { _exit(errno); }

    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;           // ms -> s
    tv.tv_usec = (milliseconds % 1000) * 1000; // leftover ms -> us

    // Select with no file descriptors can be used as a makeshift sleep function
    // (that can still be interrupted by SIGTERM)
    errno = 0;
    if (select(0, NULL, NULL, NULL, &tv) == -1) { _exit(errno); }

    _exit(0);
  }

  errno = 0;
  if (setpgid(timeout_pid, pid) == -1) {
    // EACCES should not occur since we don't call execve in the timeout process
    return REPROC_UNKNOWN_ERROR;
  };

  // -reproc->pid waits for all processes in the reproc->pid process group
  // which in this case will be the process we want to wait for and the timeout
  // process. waitpid will return the process id of whichever process exits
  // first
  int status = EXIT_STATUS_NULL;
  errno = 0;
  pid_t exit_pid = waitpid(-pid, &status, 0);

  // If the timeout process exits first the timeout will have been exceeded
  if (exit_pid == timeout_pid) { return REPROC_WAIT_TIMEOUT; }

  // If the child process exits first we make sure the timeout process is
  // cleaned up correctly by sending a SIGTERM signal and waiting for it
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
  // while waiting for the timeout process/child process
  if (exit_pid == -1) {
    switch (errno) {
    case EINTR: return REPROC_INTERRUPTED;
    default: return REPROC_UNKNOWN_ERROR;
    }
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}


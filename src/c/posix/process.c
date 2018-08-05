#include "wait.h"

#include "fork.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned int parse_exit_status(int status)
{
  // WEXITSTATUS returns a value between [0,256) so casting to unsigned int is
  // safe

  if (WIFEXITED(status)) { return (unsigned int) WEXITSTATUS(status); }

  assert(WIFSIGNALED(status));

  return (unsigned int) WTERMSIG(status);
}

static REPROC_ERROR wait_no_hang(pid_t pid, unsigned int *exit_status)
{
  int status = 0;
  errno = 0;
  // Adding WNOHANG makes waitpid only check and return immediately
  pid_t wait_result = waitpid(pid, &status, WNOHANG);

  if (wait_result == 0) { return REPROC_WAIT_TIMEOUT; }
  if (wait_result == -1) {
    // Ignore EINTR, it shouldn't happen when using WNOHANG
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

// See Design section in README.md for an explanation of how this works
static REPROC_ERROR wait_timeout(pid_t pid, unsigned int timeout,
                                 unsigned int *exit_status)
{
  assert(timeout > 0);

  REPROC_ERROR error = REPROC_SUCCESS;

  // Check if child process hasn't exited already before starting timeout fork
  error = wait_no_hang(pid, exit_status);
  if (error != REPROC_WAIT_TIMEOUT) { return error; }

  pid_t timeout_pid = 0;
  error = fork_timeout(timeout, pid, &timeout_pid);
  if (error) { return error; }

  // -reproc->pid waits for all processes in the reproc->pid process group
  // which in this case will be the process we want to wait for and the timeout
  // process. waitpid will return the process id of whichever process exits
  // first
  int status = 0;
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

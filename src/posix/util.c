#include "util.h"

#include <assert.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

PROCESS_LIB_ERROR system_error_to_process_error(int system_error)
{
  switch (system_error) {
  case 0: return PROCESS_LIB_SUCCESS;
  default: return PROCESS_LIB_UNKNOWN_ERROR;
  }
}

PROCESS_LIB_ERROR pipe_init(int *read, int *write)
{
  int pipefd[2];

  errno = 0;

  int result = pipe(pipefd);

  if (result == -1) { return system_error_to_process_error(errno); }

  // Assign file desccriptors if create pipe was succesful
  *read = pipefd[0];
  *write = pipefd[1];

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer, uint32_t to_write,
                             uint32_t *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  errno = 0;

  *actual = write(pipe, buffer, to_write);

  // write returns -1 on error which is the max unsigned value
  if (*actual == UINT32_MAX) { return system_error_to_process_error(errno); }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, uint32_t to_read,
                            uint32_t *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  errno = 0;

  *actual = read(pipe, buffer, to_read);

  if (*actual == UINT32_MAX) { return system_error_to_process_error(errno); }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_close(int *pipe_address)
{
  assert(pipe_address);

  int pipe = *pipe_address;

  // Do nothing and return success on null pipe (0) so callers don't have to
  // check each time if a pipe has been closed already
  if (!pipe) { return PROCESS_LIB_SUCCESS; }

  errno = 0;

  int result = close(pipe);
  *pipe_address = 0; // Resources should only be closed once

  if (result == -1) { return system_error_to_process_error(errno); }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR wait_no_hang(pid_t pid, int *exit_status)
{
  assert(pid);
  assert(exit_status);

  errno = 0;

  int status = 0;
  pid_t wait_result = waitpid(pid, &status, WNOHANG);

  if (wait_result == 0) { return PROCESS_LIB_WAIT_TIMEOUT; }
  if (wait_result == -1) { return system_error_to_process_error(errno); }

  *exit_status = parse_exit_status(status);

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR wait_infinite(pid_t pid, int *exit_status)
{
  assert(pid);
  assert(exit_status);

  errno = 0;

  int status = 0;
  pid_t wait_result = waitpid(pid, &status, 0);

  if (wait_result == -1) { return system_error_to_process_error(errno); }

  *exit_status = parse_exit_status(status);

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR wait_timeout(pid_t pid, int *exit_status,
                               uint32_t milliseconds)
{
  assert(pid);
  assert(exit_status);
  assert(milliseconds > 0);

  errno = 0;

  pid_t timeout_pid = fork();

  if (timeout_pid == -1) { return system_error_to_process_error(errno); }

  if (timeout_pid == 0) {
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;           // ms -> s
    tv.tv_usec = (milliseconds % 1000) * 1000; // leftover ms -> us

    // Select with no fd's can be used as a makeshift sleep function
    select(0, NULL, NULL, NULL, &tv);

    _exit(0);
  }

  // Set process group to the same process group of the process we're waiting
  // for
  if (setpgid(timeout_pid, pid) == -1) {
    return system_error_to_process_error(errno);
  };

  // -process->pid waits for all processes in the process->pid process group
  // which in this case will be the process we want to wait for and the timeout
  // process. waitpid will return the process id of whichever process exits
  // first.
  int status = 0;
  pid_t exit_pid = waitpid(-pid, &status, 0);

  if (exit_pid == -1) { return system_error_to_process_error(errno); }
  // If the timeout process exits first the timeout will have been exceeded
  if (exit_pid == timeout_pid) { return PROCESS_LIB_WAIT_TIMEOUT; }

  *exit_status = parse_exit_status(status);

  return PROCESS_LIB_SUCCESS;
}

int32_t parse_exit_status(int status)
{
  if (WIFEXITED(status)) { return WEXITSTATUS(status); }

  assert(WIFSIGNALED(status));
  return WTERMSIG(status);
}

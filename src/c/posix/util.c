#include "util.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

PROCESS_LIB_ERROR pipe_init(int *read, int *write)
{
  assert(read);
  assert(write);

  int pipefd[2];

  errno = 0;
#ifdef __APPLE__
  int result = pipe(pipefd);
  fcntl(pipefd[0], FD_SET, FD_CLOEXEC);
  fcntl(pipefd[0], FD_SET, FD_CLOEXEC);
#else
  int result = pipe2(pipefd, O_CLOEXEC);
#endif

  if (result) {
    switch (errno) {
    case ENFILE: return PROCESS_LIB_PIPE_LIMIT_REACHED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  // Assign file descriptors if pipe() call was succesful
  *read = pipefd[0];
  *write = pipefd[1];

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer,
                             unsigned int to_write, unsigned int *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  errno = 0;
  ssize_t bytes_written = write(pipe, buffer, to_write);
  *actual = 0; // changed to bytes_written if write was succesful

  if (bytes_written == -1) {
    switch (errno) {
    case EPIPE: return PROCESS_LIB_STREAM_CLOSED;
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    case EIO: return PROCESS_LIB_IO_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *actual = (unsigned int) bytes_written;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, unsigned int to_read,
                            unsigned int *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  errno = 0;
  ssize_t bytes_read = read(pipe, buffer, to_read);
  *actual = 0; // changed to bytes_read if read was succesful

  if (bytes_read == 0) { return PROCESS_LIB_STREAM_CLOSED; }
  if (bytes_read == -1) {
    switch (errno) {
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    case EIO: return PROCESS_LIB_IO_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *actual = (unsigned int) bytes_read;

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
  // Close should not be repeated on error so always set pipe to 0 after close
  *pipe_address = 0;

  if (result == -1) {
    switch (errno) {
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    case EIO: return PROCESS_LIB_IO_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR wait_no_hang(pid_t pid, int *exit_status)
{
  assert(pid);
  assert(exit_status);

  int status = 0;
  errno = 0;
  pid_t wait_result = waitpid(pid, &status, WNOHANG);

  if (wait_result == 0) { return PROCESS_LIB_WAIT_TIMEOUT; }
  if (wait_result == -1) {
    switch (errno) {
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *exit_status = parse_exit_status(status);

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR wait_infinite(pid_t pid, int *exit_status)
{
  assert(pid);
  assert(exit_status);

  int status = 0;
  errno = 0;
  pid_t wait_result = waitpid(pid, &status, 0);

  if (wait_result == -1) {
    switch (errno) {
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *exit_status = parse_exit_status(status);

  return PROCESS_LIB_SUCCESS;
}

// See Design section in README.md for an explanation of how this works
PROCESS_LIB_ERROR wait_timeout(pid_t pid, int *exit_status,
                               unsigned int milliseconds)
{
  assert(pid);
  assert(exit_status);
  assert(milliseconds > 0);

  errno = 0;
  pid_t timeout_pid = fork();

  if (timeout_pid == -1) {
    switch (errno) {
    case EAGAIN: return PROCESS_LIB_PROCESS_LIMIT_REACHED;
    case ENOMEM: return PROCESS_LIB_MEMORY_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
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
    return PROCESS_LIB_UNKNOWN_ERROR;
  };

  // -process->pid waits for all processes in the process->pid process group
  // which in this case will be the process we want to wait for and the timeout
  // process. waitpid will return the process id of whichever process exits
  // first
  int status = 0;
  errno = 0;
  pid_t exit_pid = waitpid(-pid, &status, 0);

  // If the timeout process exits first the timeout will have been exceeded
  if (exit_pid == timeout_pid) { return PROCESS_LIB_WAIT_TIMEOUT; }

  // If the child process exits first we make sure the timeout process is
  // cleaned up correctly by sending a SIGTERM signal and waiting for it
  errno = 0;
  if (kill(timeout_pid, SIGTERM) == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  errno = 0;
  if (waitpid(timeout_pid, NULL, 0) == -1) {
    switch (errno) {
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  // After cleaning up the timeout process we can check if an error occurred
  // while waiting for the timeout process/child process
  if (exit_pid == -1) {
    switch (errno) {
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  *exit_status = parse_exit_status(status);

  return PROCESS_LIB_SUCCESS;
}

int parse_exit_status(int status)
{
  if (WIFEXITED(status)) { return WEXITSTATUS(status); }

  assert(WIFSIGNALED(status));
  return WTERMSIG(status);
}

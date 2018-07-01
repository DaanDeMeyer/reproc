#include "util.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <spawn.h>

static PROCESS_LIB_ERROR posix_spawn_setup(posix_spawnattr_t *attributes,
                                           posix_spawn_file_actions_t *actions,
                                           int stdin_pipe, int stdout_pipe,
                                           int stderr_pipe);

static PROCESS_LIB_ERROR fork_posix_spawn(const char *argv[],
                                          const char *working_directory,
                                          posix_spawnattr_t *attributes,
                                          posix_spawn_file_actions_t *actions,
                                          pid_t *pid, int *spawn_error);

PROCESS_LIB_ERROR fork_exec_redirect(const char *argv[],
                                     const char *working_directory,
                                     int stdin_pipe, int stdout_pipe,
                                     int stderr_pipe, pid_t *pid)
{
  PROCESS_LIB_ERROR error = PROCESS_LIB_SUCCESS;

  posix_spawnattr_t attributes;
  posix_spawn_file_actions_t actions;
  error = process_spawn_setup(&attributes, &actions, stdin_pipe, stdout_pipe,
                              stderr_pipe);
  if (error) {
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
  }

  int spawn_error = 0;

  if (!working_directory) {
    spawn_error = posix_spawnp(pid, argv[0], &actions, &attributes,
                               (char **) argv, NULL);
  } else {
    error = fork_posix_spawn(argv, working_directory, &attributes, &actions,
                             pid, &spawn_error);
  }

  posix_spawnattr_destroy(&attributes);
  posix_spawn_file_actions_destroy(&actions);

  if (error) { return error; }

  if (spawn_error != 0) {
    switch (spawn_error) {
    case EACCES: return PROCESS_LIB_PERMISSION_DENIED;
    case EPERM: return PROCESS_LIB_PERMISSION_DENIED;
    case EIO: return PROCESS_LIB_IO_ERROR;
    case ELOOP: return PROCESS_LIB_SYMLINK_LOOP;
    case ENOMEM: return PROCESS_LIB_MEMORY_ERROR;
    case ENOENT: return PROCESS_LIB_FILE_NOT_FOUND;
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    }
  }

  return PROCESS_LIB_SUCCESS;
}

short flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT;

static PROCESS_LIB_ERROR posix_spawn_setup(posix_spawnattr_t *attributes,
                                           posix_spawn_file_actions_t *actions,
                                           int stdin_pipe, int stdout_pipe,
                                           int stderr_pipe)
{
  int error = 0;

  error = posix_spawnattr_init(attributes);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  posix_spawnattr_setflags(attributes, flags);
  posix_spawnattr_setpgroup(attributes, 0);

  error = posix_spawn_file_actions_init(actions);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  error = posix_spawn_file_actions_adddup2(actions, stdin_pipe, STDIN_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }
  error = posix_spawn_file_actions_adddup2(actions, stdout_pipe, STDOUT_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }
  error = posix_spawn_file_actions_adddup2(actions, stderr_pipe, STDERR_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  return PROCESS_LIB_SUCCESS;
}

static PROCESS_LIB_ERROR fork_posix_spawn(const char *argv[],
                                          const char *working_directory,
                                          posix_spawnattr_t *attributes,
                                          posix_spawn_file_actions_t *actions,
                                          pid_t *pid, int *spawn_error)
{
  PROCESS_LIB_ERROR error;

  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) { return error; }

  errno = 0;
  pid_t chdir_pid = fork();
  if (chdir_pid == -1) {
    pipe_close(&error_pipe_read);
    pipe_close(&error_pipe_write);

    switch (errno) {
    case EAGAIN: return PROCESS_LIB_PROCESS_LIMIT_REACHED;
    case ENOMEM: return PROCESS_LIB_MEMORY_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  if (chdir_pid == 0) {
    errno = 0;

    if (chdir(working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // TODO check envp
    errno = posix_spawnp(pid, argv[0], actions, attributes, (char **) argv,
                         NULL);
    write(error_pipe_write, &errno, sizeof(errno));
    _exit(errno);
  }

  // Close write end on parent side so read will read 0 if it is closed on the
  // child side as well
  pipe_close(&error_pipe_write);
  ssize_t bytes_read = read(error_pipe_read, &spawn_error, sizeof(spawn_error));
  pipe_close(&error_pipe_read);

  if (waitpid(chdir_pid, NULL, 0) == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  if (bytes_read == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  return PROCESS_LIB_SUCCESS;
}
#else
PROCESS_LIB_ERROR fork_exec_redirect(const char *argv[],
                                     const char *working_directory,
                                     int stdin_pipe, int stdout_pipe,
                                     int stderr_pipe, pid_t *pid)
{
  PROCESS_LIB_ERROR error;

  // We create an error pipe to receive pre-exec and exec errors from the child
  // process. See this answer https://stackoverflow.com/a/1586277 for more
  // information
  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) { return error; }

  errno = 0;
  *pid = fork();

  if (*pid == -1) {
    pipe_close(&error_pipe_read);
    pipe_close(&error_pipe_write);

    switch (errno) {
    case EAGAIN: return PROCESS_LIB_PROCESS_LIMIT_REACHED;
    case ENOMEM: return PROCESS_LIB_MEMORY_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  if (*pid == 0) {
    // In child process (Since we're in the child process we can exit on error)
    // Why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    errno = 0;

    // We put the child process in its own process group which is needed by
    // process_wait. Normally there might be a race condition if the parent
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

    // Pipes are created with FD_CLOEXEC which results in them getting
    // automatically closed on exec so we don't need to close them manually in
    // the child process

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

  ssize_t bytes_read = read(error_pipe_read, &errno, sizeof(errno));

  pipe_close(&error_pipe_read);

  // If read does not return 0 an error will have occurred in the child process
  // before execve (or an error with read itself (less likely))
  if (bytes_read != 0) {
    switch (errno) {
    case EACCES: return PROCESS_LIB_PERMISSION_DENIED;
    case EPERM: return PROCESS_LIB_PERMISSION_DENIED;
    case EIO: return PROCESS_LIB_IO_ERROR;
    case ELOOP: return PROCESS_LIB_SYMLINK_LOOP;
    case ENOMEM: return PROCESS_LIB_MEMORY_ERROR;
    case ENOENT: return PROCESS_LIB_FILE_NOT_FOUND;
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    }
  }

  return PROCESS_LIB_SUCCESS;
}
#endif

PROCESS_LIB_ERROR pipe_init(int *read, int *write)
{
  assert(read);
  assert(write);

  int pipefd[2];

  errno = 0;
  // See Gotcha's in the readme for a detailed explanation
#ifdef __APPLE__
  int result = pipe(pipefd);
  fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
#else
  int result = pipe2(pipefd, O_CLOEXEC);
#endif

  if (result == -1) {
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

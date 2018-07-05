#include "fork_exec_redirect.h"

#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

PROCESS_LIB_ERROR fork_exec_redirect(int argc, const char *argv[],
                                     const char *working_directory,
                                     int stdin_pipe, int stdout_pipe,
                                     int stderr_pipe, pid_t *pid)
{
  assert(argc > 0);
  assert(argv);
  assert(argv[argc]);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  assert(stdin_pipe);
  assert(stdout_pipe);
  assert(stderr_pipe);
  assert(pid);

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

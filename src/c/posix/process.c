#include "process.h"
#include "util.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

struct process {
  pid_t pid;
  int parent_stdin;
  int parent_stdout;
  int parent_stderr;
  int child_stdin;
  int child_stdout;
  int child_stderr;
  int exit_status;
};

unsigned int process_size()
{
  // process struct is small so its size should always fit inside unsigned int
  return (unsigned int) sizeof(struct process);
}

PROCESS_LIB_ERROR process_init(struct process *process)
{
  assert(process);

  // process id 0 is reserved by the system so we can use it as a null value
  process->pid = 0;
  // File descriptor 0 won't be assigned by pipe() call so we use it as a null
  // value
  process->parent_stdin = 0;
  process->parent_stdout = 0;
  process->parent_stderr = 0;
  process->child_stdin = 0;
  process->child_stdout = 0;
  process->child_stderr = 0;
  // Exit codes on unix are in range [0,256) so we can use -1 as a null value.
  // We save the exit status because after calling waitpid multiple times on a
  // process that has already exited is unsafe since after the first time the
  // system can reuse the process id for another process.
  process->exit_status = -1;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_start(struct process *process, const char *argv[],
                                int argc, const char *working_directory)
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  // Make sure process_start is only called once for each process_init call
  assert(!process->pid);

  PROCESS_LIB_ERROR error;
  error = pipe_init(&process->child_stdin, &process->parent_stdin);
  if (error) { return error; }
  error = pipe_init(&process->parent_stdout, &process->child_stdout);
  if (error) { return error; }
  error = pipe_init(&process->parent_stderr, &process->child_stderr);
  if (error) { return error; }

  // We create an error pipe to receive pre-exec and exec errors from the child
  // process. See the accepted answer of
  // https://stackoverflow.com/questions/1584956/how-to-handle-execvp-errors-after-fork
  // for more information
  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) { return error; }

  // We put the child process in its own process group which is needed by
  // process_wait The process group is set in both parent and child to avoid
  // race conditions (see setpgid calls)

  errno = 0;
  process->pid = fork();

  if (process->pid == -1) {
    switch (errno) {
    case EAGAIN: return PROCESS_LIB_PROCESS_LIMIT_REACHED;
    case ENOMEM: return PROCESS_LIB_MEMORY_ERROR;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  if (process->pid == 0) {
    // In child process (Since we're in the child process we can exit on error)
    // Why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    errno = 0;

    // Normally there might be a race condition if the parent process waits for
    // the child process before the child process puts itself in its own
    // process group (with setpgid) but this is avoided because we always read
    // from the error pipe in the parent process after forking. When read
    // returns the child process will either have errored out (and waiting won't
    // be valid) or will have executed execvp (and as a result setpgid as well)
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
    if (dup2(process->child_stdin, STDIN_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (dup2(process->child_stdout, STDOUT_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (dup2(process->child_stderr, STDERR_FILENO) == -1) {
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
  error = pipe_close(&error_pipe_write);
  if (error) { return error; }

  // If read does not return 0 an error will have occurred in the child process
  // before execve (or an error with read itself (less likely))
  if (read(error_pipe_read, &errno, sizeof(errno)) != 0) {
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
  // Error pipe is not necessary anymore (write end is closed automatically on
  // exec)
  error = pipe_close(&error_pipe_read);
  if (error) { return error; }

  error = pipe_close(&process->child_stdin);
  if (error) { return error; }
  error = pipe_close(&process->child_stdout);
  if (error) { return error; }
  error = pipe_close(&process->child_stderr);
  if (error) { return error; }

  return error;
}

PROCESS_LIB_ERROR process_write(struct process *process, const void *buffer,
                                unsigned int to_write, unsigned int *actual)
{
  assert(process);
  assert(process->parent_stdin);
  assert(buffer);
  assert(actual);

  return pipe_write(process->parent_stdin, buffer, to_write, actual);
}

PROCESS_LIB_ERROR process_close_stdin(struct process *process)
{
  assert(process);
  assert(process->parent_stdin);

  return pipe_close(&process->parent_stdin);
}

PROCESS_LIB_ERROR process_read(struct process *process, void *buffer,
                               unsigned int to_read, unsigned int *actual)
{
  assert(process);
  assert(process->parent_stdout);
  assert(buffer);
  assert(actual);

  return pipe_read(process->parent_stdout, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_read_stderr(struct process *process, void *buffer,
                                      unsigned int to_read,
                                      unsigned int *actual)
{
  assert(process);
  assert(process->parent_stderr);
  assert(buffer);
  assert(actual);

  return pipe_read(process->parent_stderr, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_wait(struct process *process,
                               unsigned int milliseconds)
{
  assert(process);
  assert(process->pid);

  // Don't wait if child process has already exited. We don't use waitpid for
  // this because if we've already waited once after the process has exited the
  // pid of the process might have already been reused by the system.
  if (process->exit_status != -1) { return PROCESS_LIB_SUCCESS; }

  if (milliseconds == 0) {
    return wait_no_hang(process->pid, &process->exit_status);
  }

  if (milliseconds == PROCESS_LIB_INFINITE) {
    return wait_infinite(process->pid, &process->exit_status);
  }

  return wait_timeout(process->pid, &process->exit_status, milliseconds);
}

PROCESS_LIB_ERROR process_terminate(struct process *process,
                                    unsigned int milliseconds)
{
  assert(process);
  assert(process->pid);

  // Check if child process has already exited before sending signal
  PROCESS_LIB_ERROR error = process_wait(process, 0);

  // Return if wait succeeds (which means process has already exited) or if
  // an error other than a wait timeout occurs during waiting
  if (error != PROCESS_LIB_WAIT_TIMEOUT) { return error; }

  errno = 0;
  if (kill(process->pid, SIGTERM) == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_kill(struct process *process,
                               unsigned int milliseconds)
{
  assert(process);
  assert(process->pid);

  // Check if child process has already exited before sending signal
  PROCESS_LIB_ERROR error = process_wait(process, 0);

  // Return if wait succeeds (which means process has already exited) or if
  // an error other than a wait timeout occurs during waiting
  if (error != PROCESS_LIB_WAIT_TIMEOUT) { return error; }

  errno = 0;
  if (kill(process->pid, SIGKILL) == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_exit_status(struct process *process, int *exit_status)
{
  assert(process);

  if (process->exit_status == -1) { return PROCESS_LIB_STILL_RUNNING; }

  *exit_status = process->exit_status;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_free(struct process *process)
{
  assert(process);

  // All resources are closed regardless of errors but only the first error
  // is returned
  PROCESS_LIB_ERROR result = PROCESS_LIB_SUCCESS;
  PROCESS_LIB_ERROR error;

  error = pipe_close(&process->parent_stdin);
  if (!result) { result = error; }
  error = pipe_close(&process->parent_stdout);
  if (!result) { result = error; }
  error = pipe_close(&process->parent_stderr);
  if (!result) { result = error; }

  error = pipe_close(&process->child_stdin);
  if (!result) { result = error; }
  error = pipe_close(&process->child_stdout);
  if (!result) { result = error; }
  error = pipe_close(&process->child_stderr);
  if (!result) { result = error; }

  return result;
}

unsigned int process_system_error(void) { return (unsigned int) errno; }

#include "process.h"
#include "util.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct process {
  pid_t pid;
  int stdin;
  int stdout;
  int stderr;
  int child_stdin;
  int child_stdout;
  int child_stderr;
  int exit_status;
};

PROCESS_LIB_ERROR process_init(struct process **process_address)
{
  assert(process_address);

  *process_address = malloc(sizeof(struct process));

  struct process *process = *process_address;
  if (!process) { return PROCESS_LIB_MEMORY_ERROR; }

  process->pid = 0;

  // File descriptor 0 won't be assigned by pipe() call so we use it as a null
  // value
  process->stdin = 0;
  process->stdout = 0;
  process->stderr = 0;
  process->child_stdin = 0;
  process->child_stdout = 0;
  process->child_stderr = 0;
  process->exit_status = -1; // Exit codes on unix are in range [0,256)

  PROCESS_LIB_ERROR error;
  error = pipe_init(&process->child_stdin, &process->stdin);
  if (error) { return error; }
  error = pipe_init(&process->stdout, &process->child_stdout);
  if (error) { return error; }
  error = pipe_init(&process->stderr, &process->child_stderr);
  if (error) { return error; }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_start(struct process *process, int argc,
                                const char *argv[],
                                const char *working_directory)
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  assert(!process->pid);

  assert(process->stdin);
  assert(process->stdout);
  assert(process->stderr);
  assert(process->child_stdin);
  assert(process->child_stdout);
  assert(process->child_stderr);
  assert(process->exit_status == -1);

  // We put process in its own process group which is needed by process_wait.
  // The process group is set in both parent and child to avoid race conditions

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
    // In child process
    // Since we're in a child process we can exit on error
    // why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    errno = 0;

    if (setpgid(0, 0) == -1) { _exit(errno); }

    if (working_directory && chdir(working_directory) == -1) { _exit(errno); }

    // redirect stdin, stdout and stderr
    // _exit ensures open file descriptors (pipes) are closed
    if (dup2(process->child_stdin, STDIN_FILENO) == -1) { _exit(errno); }
    if (dup2(process->child_stdout, STDOUT_FILENO) == -1) { _exit(errno); }
    if (dup2(process->child_stderr, STDERR_FILENO) == -1) { _exit(errno); }

    // We copied the pipes to the actual streams (stdin/stdout/stderr) so we
    // don't need the originals anymore
    if (close(process->child_stdin) == -1) { _exit(errno); };
    if (close(process->child_stdout) == -1) { _exit(errno); };
    if (close(process->child_stderr) == -1) { _exit(errno); };

    // We also have no use for the parent endpoints of the pipes in the child
    // process
    if (close(process->stdin) == -1) { _exit(errno); };
    if (close(process->stdout) == -1) { _exit(errno); };
    if (close(process->stderr) == -1) { _exit(errno); };

    // Replace forked child with process we want to run
    // Safe cast (execvp doesn't actually change the contents of argv)
    execvp(argv[0], (char **) argv);

    // Exit if execvp fails
    _exit(errno);
  }

  errno = 0;
  if (setpgid(process->pid, 0) == -1) {
    switch (errno) {
    // If we get EACCESS the child process has already executed execvp which
    // means it also has executed setpgid(0, 0) which means the process group
    // is set correctly so EACCES isn't an error for us in this case.
    case EACCES: break;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  PROCESS_LIB_ERROR error;
  error = pipe_close(&process->child_stdin);
  if (error) { return error; }
  error = pipe_close(&process->child_stdout);
  if (error) { return error; }
  error = pipe_close(&process->child_stderr);
  if (error) { return error; }

  return error;
}

PROCESS_LIB_ERROR process_write(struct process *process, const void *buffer,
                                uint32_t to_write, uint32_t *actual)
{
  assert(process);
  assert(process->stdin);
  assert(buffer);
  assert(actual);

  return pipe_write(process->stdin, buffer, to_write, actual);
}

PROCESS_LIB_ERROR process_read(struct process *process, void *buffer,
                               uint32_t to_read, uint32_t *actual)
{
  assert(process);
  assert(process->stdout);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stdout, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_read_stderr(struct process *process, void *buffer,
                                      uint32_t to_read, uint32_t *actual)
{
  assert(process);
  assert(process->stderr);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stderr, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_wait(struct process *process, uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

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
                                    uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

  PROCESS_LIB_ERROR error = process_wait(process, 0);
  // Return if wait succeeds (which means process has already exited) or if
  // an error other than a wait timeout occurs during waiting
  if (error != PROCESS_LIB_WAIT_TIMEOUT) { return error; }

  errno = 0;
  if (kill(process->pid, SIGTERM) == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_kill(struct process *process, uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

  PROCESS_LIB_ERROR error = process_wait(process, 0);
  // Return if wait succeeds (which means process has already exited) or if
  // an error other than a wait timeout occurs during waiting
  if (error != PROCESS_LIB_WAIT_TIMEOUT) { return error; }

  errno = 0;
  if (kill(process->pid, SIGKILL) == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_exit_status(struct process *process,
                                      int32_t *exit_status)
{
  assert(process);

  if (process->exit_status == -1) { return PROCESS_LIB_STILL_RUNNING; }

  *exit_status = process->exit_status;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_free(struct process **process_address)
{
  assert(process_address);
  assert(*process_address);

  struct process *process = *process_address;

  // All resources are closed regardless of errors but only the first error
  // is returned
  PROCESS_LIB_ERROR result = PROCESS_LIB_SUCCESS;
  PROCESS_LIB_ERROR error;

  error = pipe_close(&process->stdin);
  if (!result) { result = error; }
  error = pipe_close(&process->stdout);
  if (!result) { result = error; }
  error = pipe_close(&process->stderr);
  if (!result) { result = error; }

  error = pipe_close(&process->child_stdin);
  if (!result) { result = error; }
  error = pipe_close(&process->child_stdout);
  if (!result) { result = error; }
  error = pipe_close(&process->child_stderr);
  if (!result) { result = error; }

  free(process);
  *process_address = NULL;

  return result;
}

int64_t process_system_error(void) { return errno; }

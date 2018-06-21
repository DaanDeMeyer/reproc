#include "process.h"
#include "util.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <signal.h>
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
};

Process process_alloc(void)
{
  return malloc(sizeof(struct process));;
}

PROCESS_ERROR process_init(Process process)
{
  assert(process);

  process->pid = 0;

  // File descriptor 0 won't be assigned by pipe() call so we use it as a null
  // value
  process->stdin = 0;
  process->stdout = 0;
  process->stderr = 0;
  process->child_stdin = 0;
  process->child_stdout = 0;
  process->child_stderr = 0;

  PROCESS_ERROR error = PROCESS_SUCCESS;
  error = error ? error : pipe_init(&process->child_stdin, &process->stdin);
  error = error ? error : pipe_init(&process->stdout, &process->child_stdout);
  error = error ? error : pipe_init(&process->stderr, &process->child_stderr);

  return error;
}

PROCESS_ERROR process_start(Process process, int argc, char *argv[])
{
  assert(process);

  assert(argc > 0);
  assert(argv != NULL);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i] != NULL);
  }

  assert(!process->pid);

  assert(process->stdin);
  assert(process->stdout);
  assert(process->stderr);
  assert(process->child_stdin);
  assert(process->child_stdout);
  assert(process->child_stderr);

  errno = 0;

  process->pid = fork();
  if (process->pid == 0) {
    // In child process
    // Since we're in a child process we can exit on error
    // why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    // Put process in its own process group which is needed by process_wait
    setpgid(0, 0);

    // redirect stdin, stdout and stderr
    // _exit ensures open file descriptors (pipes) are closed
    if (dup2(process->child_stdin, STDIN_FILENO) == -1) { _exit(errno); }
    if (dup2(process->child_stdout, STDOUT_FILENO) == -1) { _exit(errno); }
    if (dup2(process->child_stderr, STDERR_FILENO) == -1) { _exit(errno); }

    // We copied the pipes to the actual streams (stdin/stdout/stderr) so we
    // don't need the originals anymore
    close(process->child_stdin);
    close(process->child_stdout);
    close(process->child_stderr);

    // We also have no use for the parent endpoints of the pipes in the child
    // process
    close(process->stdin);
    close(process->stdout);
    close(process->stderr);

    // Replace forked child with process we want to run
    execvp(argv[0], argv);

    // Exit if execvp fails
    _exit(errno);
  }

  PROCESS_ERROR error = system_error_to_process_error(errno);

  close(process->child_stdin);
  close(process->child_stdout);
  close(process->child_stderr);

  process->child_stdin = 0;
  process->child_stdout = 0;
  process->child_stderr = 0;

  error = error ? error : system_error_to_process_error(errno);

  return error;
}

PROCESS_ERROR process_write(Process process, const void *buffer,
                            uint32_t to_write, uint32_t *actual)
{
  assert(process);
  assert(process->stdin);
  assert(buffer);
  assert(actual);

  return pipe_write(process->stdin, buffer, to_write, actual);
}

PROCESS_ERROR process_read(Process process, void *buffer, uint32_t to_read,
                           uint32_t *actual)
{
  assert(process);
  assert(process->stdout);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stdout, buffer, to_read, actual);
}

PROCESS_ERROR process_read_stderr(Process process, void *buffer,
                                  uint32_t to_read, uint32_t *actual)
{
  assert(process);
  assert(process->stderr);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stderr, buffer, to_read, actual);
}

PROCESS_ERROR process_wait(Process process, uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

  errno = 0;

  if (milliseconds == 0) {
    pid_t wait_result = waitpid(process->pid, NULL, WNOHANG);

    switch (wait_result) {
    case 0:
      return PROCESS_WAIT_TIMEOUT;
    case -1:
      return system_error_to_process_error(errno);
    default:
      return PROCESS_SUCCESS;
    }
  }

  if (milliseconds == INFINITE) {
    return waitpid(process->pid, NULL, 0) > 0
               ? PROCESS_SUCCESS
               : system_error_to_process_error(errno);
  }

  // 0 < milliseconds < INFINITE

  pid_t timeout_pid = fork();
  if (timeout_pid == 0) {
    // Set process group to the same process group of the process we're waiting
    // for
    setpgid(0, process->pid);

    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;

    select(0, NULL, NULL, NULL, &tv);

    _exit(0);
  }

  // -process->pid waits for all processes in the process->pid process group
  // which in this case will be the process we want to wait for and the timeout
  // process. waitpid will return the process id of whichever process exits
  // first.
  pid_t exit_pid = waitpid(-process->pid, NULL, 0);

  // If the timeout process exits first the timeout will have been exceeded
  if (exit_pid == timeout_pid) { return PROCESS_WAIT_TIMEOUT; }

  return PROCESS_SUCCESS;
}

PROCESS_ERROR process_terminate(Process process, uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

  errno = 0;

  if (kill(process->pid, SIGTERM) == -1) {
    return system_error_to_process_error(errno);
  }

  return process_wait(process, milliseconds);
}

PROCESS_ERROR process_kill(Process process, uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

  errno = 0;

  if (kill(process->pid, SIGKILL) == -1) {
    return system_error_to_process_error(errno);
  }

  return process_wait(process, milliseconds);
}

PROCESS_ERROR process_free(Process process)
{
  assert(process);

  errno = 0;

  if (process->stdin) { close(process->stdin); }
  if (process->stdout) { close(process->stdout); }
  if (process->stderr) { close(process->stderr); }

  if (process->child_stdin) { close(process->child_stdin); }
  if (process->child_stdout) { close(process->child_stdout); }
  if (process->child_stderr) { close(process->child_stderr); }

  free(process);

  PROCESS_ERROR error = system_error_to_process_error(errno);

  return error;
}

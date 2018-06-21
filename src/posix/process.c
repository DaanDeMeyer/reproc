#include "process.h"
#include "process_impl.h"
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

Process process_alloc(void) { return malloc(sizeof(struct process)); }

PROCESS_LIB_ERROR process_init(Process process)
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

  PROCESS_LIB_ERROR error = PROCESS_LIB_SUCCESS;
  error = pipe_init(&process->child_stdin, &process->stdin);
  if (error) { return error; }
  error = pipe_init(&process->stdout, &process->child_stdout);
  if (error) { return error; }
  error = pipe_init(&process->stderr, &process->child_stderr);
  if (error) { return error; }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_start(Process process, int argc, char *argv[])
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

  PROCESS_LIB_ERROR error = system_error_to_process_error(errno);

  close(process->child_stdin);
  close(process->child_stdout);
  close(process->child_stderr);

  process->child_stdin = 0;
  process->child_stdout = 0;
  process->child_stderr = 0;

  error = error ? error : system_error_to_process_error(errno);

  return error;
}

PROCESS_LIB_ERROR process_write(Process process, const void *buffer,
                                uint32_t to_write, uint32_t *actual)
{
  assert(process);
  assert(process->stdin);
  assert(buffer);
  assert(actual);

  return pipe_write(process->stdin, buffer, to_write, actual);
}

PROCESS_LIB_ERROR process_read(Process process, void *buffer, uint32_t to_read,
                               uint32_t *actual)
{
  assert(process);
  assert(process->stdout);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stdout, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_read_stderr(Process process, void *buffer,
                                      uint32_t to_read, uint32_t *actual)
{
  assert(process);
  assert(process->stderr);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stderr, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_wait(Process process, uint32_t milliseconds)
{
  assert(process);

  if (milliseconds == 0) {
    return wait_no_hang(process);
  }

  if (milliseconds == INFINITE) {
    return wait_infinite(process);
  }

  return wait_timeout(process, milliseconds);
}

PROCESS_LIB_ERROR process_terminate(Process process, uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

  errno = 0;

  if (kill(process->pid, SIGTERM) == -1) {
    return system_error_to_process_error(errno);
  }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_kill(Process process, uint32_t milliseconds)
{
  assert(process);
  assert(process->pid);

  errno = 0;

  if (kill(process->pid, SIGKILL) == -1) {
    return system_error_to_process_error(errno);
  }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_free(Process process)
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

  PROCESS_LIB_ERROR error = system_error_to_process_error(errno);

  return error;
}

int64_t process_system_error(void) { return errno; }

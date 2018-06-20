#include "process.h"
#include "util.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
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

PROCESS_ERROR process_init(process *process)
{
  assert(process != NULL);

  process->pid = 0;

  // File descriptor 0 won't be assigned by pipe() call so we use it as a null
  // value
  process->stdin = 0;
  process->stdout = 0;
  process->stderr = 0;
  process->child_stdin = 0;
  process->child_stdout = 0;
  process->child_stderr = 0;

  // Reset system error so we don't accidentely use a previous system error
  errno = 0;

  pipe_init(&process->child_stdin, &process->stdin);
  pipe_init(&process->stdout, &process->child_stdout);
  pipe_init(&process->stderr, &process->child_stderr);

  // Check if error occurred during pipe initialization
  PROCESS_ERROR error = system_error_to_process_error(errno);

  // If error occurred we release all allocated resources
  if (error != PROCESS_SUCCESS) { process_free(process); }

  return error;
}

PROCESS_ERROR process_start(process *process, int argc, char *argv[])
{
  assert(process != NULL);
  assert(argc > 0);
  assert(argv != NULL);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i] != NULL);
  }

  errno = 0;

  process->pid = fork();
  if (process->pid == 0) {
    // In child process
    // Since we're in a child process we can exit on error
    // why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

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

  return error;
}

PROCESS_ERROR process_free(process *process)
{
  assert(process != NULL);

  if (process->stdin) { close(process->stdin); }
  if (process->stdout) { close(process->stdout); }
  if (process->stderr) { close(process->stderr); }
  if (process->child_stdin) { close(process->child_stdin); }
  if (process->child_stdout) { close(process->child_stdout); }
  if (process->child_stderr) { close(process->child_stdout); }

  return PROCESS_SUCCESS;
}

// PROCESS_ERROR process_wait(process *process, uint32_t milliseconds)
// {

// }
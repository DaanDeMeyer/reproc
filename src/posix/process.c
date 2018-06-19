#include "process.h"

#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>

typedef struct process {
  // Parent pipe endpoint fd's (write, read, read)
  int stdin_;
  int stdout_;
  int stderr_;
  char stdin_buffer[PIPE_BUF];
  char stdout_buffer[PIPE_BUF];
  char stderr_buffer[PIPE_BUF];
} process;

int process_init(process *process, int argc, char *argv[])
{
  assert(process != NULL);
  assert(argc > 0);
  assert(argv != NULL);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i] != NULL);
  }
  
  static const int PIPE_READ = 0;
  static const int PIPE_WRITE = 1;

  int stdin_pipe[2] = {-1, -1};
  int stdout_pipe[2] = {-1, -1};
  int stderr_pipe[2] = {-1, -1};

  pid_t pid;

  errno = 0;

  if (pipe(stdin_pipe) == -1) { goto end; }
  process->stdin_ = stdin_pipe[PIPE_WRITE];

  if (pipe(stdout_pipe) == -1) { goto end; }
  process->stdout_ = stdout_pipe[PIPE_READ];

  if (pipe(stderr_pipe) == -1) { goto end; }
  process->stderr_ = stderr_pipe[PIPE_READ];

  pid = fork();
  if (pid == 0) {
    // In child process
    // Since we're in a child process we can exit on error
    // why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    // redirect stdin, stdout and stderr

    if (dup2(stdin_pipe[PIPE_READ], STDIN_FILENO) == -1) {
      // _exit ensures open file descriptors (pipes) are closed
      _exit(errno);
    }

    if (dup2(stdout_pipe[PIPE_WRITE], STDOUT_FILENO) == -1) { _exit(errno); }

    if (dup2(stderr_pipe[PIPE_WRITE], STDERR_FILENO) == -1) { _exit(errno); }

    // We have no use anymore for the pipe endpoints (parent endpoints are not
    // used by child and child endpoints have been copied to stdin, stdout and
    // stderr)
    close(stdin_pipe[PIPE_READ]);
    close(stdin_pipe[PIPE_WRITE]);
    close(stdout_pipe[PIPE_READ]);
    close(stdout_pipe[PIPE_WRITE]);
    close(stderr_pipe[PIPE_READ]);
    close(stderr_pipe[PIPE_WRITE]);

    // Replace forked child with process we want to run
    execvp(argv[0], argv);

    _exit(errno);
  }

end:
  // Close child pipe endpoints (not needed by parent)
  if (stdin_pipe[PIPE_READ] != -1) { close(stdin_pipe[PIPE_READ]); }
  if (stdout_pipe[PIPE_WRITE] != -1) { close(stdout_pipe[PIPE_WRITE]); }
  if (stderr_pipe[PIPE_WRITE] != -1) { close(stderr_pipe[PIPE_WRITE]); }

  if (errno != 0) {
    // error occurred so we release all remaining allocated resources
    // parent pipe endpoints
    if (stdin_pipe[PIPE_WRITE] != -1) { close(stdin_pipe[PIPE_WRITE]); }
    if (stdout_pipe[PIPE_READ] != -1) { close(stdout_pipe[PIPE_READ]); }
    if (stderr_pipe[PIPE_READ] != -1) { close(stderr_pipe[PIPE_READ]); }
    // child proccess
    if (pid != 0) { kill(pid, SIGTERM); }
  }

  return errno;
}
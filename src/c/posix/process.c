#include "process.h"

#include "fork_exec_redirect.h"
#include "pipe.h"
#include "wait.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

const unsigned int PROCESS_LIB_INFINITE = 0xFFFFFFFF;

struct process {
  pid_t pid;
  int parent_stdin;
  int parent_stdout;
  int parent_stderr;
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
  // Exit codes on unix are in range [0,256) so we can use -1 as a null value.
  // We save the exit status because after calling waitpid multiple times on a
  // process that has already exited is unsafe since after the first time the
  // system can reuse the process id for another process.
  process->exit_status = -1;

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

  // Make sure process_start is only called once for each process_init call
  assert(!process->pid);

  // Pipe endpoints for child process. These are copied to stdin/stdout/stderr
  // when the child process is created.
  int child_stdin = 0;
  int child_stdout = 0;
  int child_stderr = 0;

  PROCESS_LIB_ERROR error;
  error = pipe_init(&child_stdin, &process->parent_stdin);
  if (error) { return error; }
  error = pipe_init(&process->parent_stdout, &child_stdout);
  if (error) { return error; }
  error = pipe_init(&process->parent_stderr, &child_stderr);
  if (error) { return error; }

  error = fork_exec_redirect(argc, argv, working_directory, child_stdin,
                             child_stdout, child_stderr, &process->pid);

  // We ignore these pipe close errors since they've already been copied to the
  // stdin/stdout/stderr of the child process (they aren't used anywhere) and
  // close shouldn't be called twice on the same file descriptor so there's
  // nothing to really do if an error happens
  pipe_close(&child_stdin);
  pipe_close(&child_stdout);
  pipe_close(&child_stderr);

  if (error) { return error; }

  return PROCESS_LIB_SUCCESS;
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
  assert(exit_status);

  if (process->exit_status == -1) { return PROCESS_LIB_STILL_RUNNING; }

  *exit_status = process->exit_status;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_destroy(struct process *process)
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

  return result;
}

unsigned int process_system_error(void) { return (unsigned int) errno; }

PROCESS_LIB_ERROR process_system_error_string(char **error_string)
{
  *error_string = strerror(errno);
  return PROCESS_LIB_SUCCESS;
}

void process_system_error_string_free(char *error_string)
{
  (void) error_string;
}

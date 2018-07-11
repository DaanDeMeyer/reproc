#include "process.h"

#include "fork_exec_redirect.h"
#include "pipe.h"
#include "wait.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

const unsigned int PROCESS_LIB_INFINITE = 0xFFFFFFFF;

static const pid_t PID_NULL = 0;
static const int PIPE_NULL = -1;
// Exit codes on unix are (should be) in range [0,256) so INT_MIN shoudl be a
// safe null value.
static const int EXIT_STATUS_NULL = INT_MIN;

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
  process->pid = PID_NULL;
  // File descriptor 0 won't be assigned by pipe() call (its reserved for stdin)
  // so we use it as a null value
  process->parent_stdin = PIPE_NULL;
  process->parent_stdout = PIPE_NULL;
  process->parent_stderr = PIPE_NULL;
  process->child_stdin = PIPE_NULL;
  process->child_stdout = PIPE_NULL;
  process->child_stderr = PIPE_NULL;

  // We save the exit status because after calling waitpid multiple times on a
  // process that has already exited is unsafe since after the first time the
  // system can reuse the process id for another process.
  process->exit_status = EXIT_STATUS_NULL;

  PROCESS_LIB_ERROR error;
  error = pipe_init(&process->child_stdin, &process->parent_stdin);
  if (error) { return error; }
  error = pipe_init(&process->parent_stdout, &process->child_stdout);
  if (error) { return error; }
  error = pipe_init(&process->parent_stderr, &process->child_stderr);
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

  // Make sure process_start is only called once for each process_init call
  // (process_init sets process->pid to PID_NULL)
  assert(process->pid == PID_NULL);

  PROCESS_LIB_ERROR error = fork_exec_redirect(argc, argv, working_directory,
                                               process->child_stdin,
                                               process->child_stdout,
                                               process->child_stderr,
                                               &process->pid);

  // (On success) The child pipe endpoints have been copied to the the
  // stdin/stdout/stderr streams of the child process. We don't need the anymore
  // in the parent process so we close them.
  pipe_close(&process->child_stdin);
  pipe_close(&process->child_stdout);
  pipe_close(&process->child_stderr);

  if (error) { return error; }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_write(struct process *process, const void *buffer,
                                unsigned int to_write, unsigned int *actual)
{
  assert(process);
  assert(process->parent_stdin != PIPE_NULL);
  assert(buffer);
  assert(actual);

  return pipe_write(process->parent_stdin, buffer, to_write, actual);
}

PROCESS_LIB_ERROR process_close_stdin(struct process *process)
{
  assert(process);
  assert(process->parent_stdin != PIPE_NULL);

  pipe_close(&process->parent_stdin);

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_read(struct process *process, void *buffer,
                               unsigned int to_read, unsigned int *actual)
{
  assert(process);
  assert(process->parent_stdout != PIPE_NULL);
  assert(buffer);
  assert(actual);

  return pipe_read(process->parent_stdout, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_read_stderr(struct process *process, void *buffer,
                                      unsigned int to_read,
                                      unsigned int *actual)
{
  assert(process);
  assert(process->parent_stderr != PIPE_NULL);
  assert(buffer);
  assert(actual);

  return pipe_read(process->parent_stderr, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_wait(struct process *process,
                               unsigned int milliseconds)
{
  assert(process);
  assert(process->pid != PID_NULL);

  // Don't wait if child process has already exited. We don't use waitpid for
  // this because if we've already waited once after the process has exited the
  // pid of the process might have already been reused by the system.
  if (process->exit_status != EXIT_STATUS_NULL) { return PROCESS_LIB_SUCCESS; }

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
  assert(process->pid != PID_NULL);

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
  assert(process->pid != PID_NULL);

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

  if (process->exit_status == EXIT_STATUS_NULL) {
    return PROCESS_LIB_STILL_RUNNING;
  }

  *exit_status = process->exit_status;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_destroy(struct process *process)
{
  assert(process);

  pipe_close(&process->parent_stdin);
  pipe_close(&process->parent_stdout);
  pipe_close(&process->parent_stderr);

  return PROCESS_LIB_SUCCESS;
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

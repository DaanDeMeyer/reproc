// See "Avoiding leaking of file descriptors and handles" in the Design chapter
// of the readme for why we use a different implementation of fork_exec_redirect
// on Darwin

#include "fork_exec_redirect.h"

#include "pipe.h"
#include "wait.h"

#include <assert.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

extern char **environ;

short flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_CLOEXEC_DEFAULT;

static PROCESS_LIB_ERROR posix_spawn_setup(posix_spawnattr_t *attributes,
                                           posix_spawn_file_actions_t *actions,
                                           int stdin_pipe, int stdout_pipe,
                                           int stderr_pipe)
{
  assert(attributes);
  assert(actions);
  assert(stdin_pipe);
  assert(stdout_pipe);
  assert(stderr_pipe);

  int error = 0;

  error = posix_spawnattr_init(attributes);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  // Put child process in its own process group so we can wait for it
  // specifically (detailed explanation in Design section in readme)
  posix_spawnattr_setflags(attributes, flags);
  posix_spawnattr_setpgroup(attributes, 0);

  error = posix_spawn_file_actions_init(actions);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  // Make sure stdin/stdout/stderr are redirected to process-lib pipes
  error = posix_spawn_file_actions_adddup2(actions, stdin_pipe, STDIN_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }
  error = posix_spawn_file_actions_adddup2(actions, stdout_pipe, STDOUT_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }
  error = posix_spawn_file_actions_adddup2(actions, stderr_pipe, STDERR_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  return PROCESS_LIB_SUCCESS;
}

int child_pid = 0;

void termination_handler(int signum)
{
  (void) signum;
  PROCESS_LIB_ERROR error;
  int exit_status;
  error = wait_no_hang(child_pid, &exit_status);
  if (!error) { _exit(exit_status); }
  kill(child_pid, SIGTERM);
  wait_infinite(child_pid, &exit_status);
  _exit(exit_status);
}

static PROCESS_LIB_ERROR fork_posix_spawn(int argc, const char *argv[],
                                          const char *working_directory,
                                          posix_spawnattr_t *attributes,
                                          posix_spawn_file_actions_t *actions,
                                          pid_t *pid, int *spawn_error)
{
  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  assert(attributes);
  assert(actions);
  assert(pid);
  assert(spawn_error);

  PROCESS_LIB_ERROR error;

  // Set up pipe for retrieving the child process pid from the forked process.
  // We don't need an error pipe since we can just wait for the forked process
  // to exit and read its exit status.
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

    // Change the working directory of the fork before calling posix_spawn.
    // The process spawned by posix_spawn will inherit this working
    // directory
    if (chdir(working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    errno = posix_spawnp(&child_pid, argv[0], actions, attributes,
                         (char **) argv, environ);
    if (errno != 0) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // Write 0 on success
    write(error_pipe_write, &errno, sizeof(errno));
    pipe_close(&error_pipe_write);

    struct sigaction action;
    action.sa_handler = termination_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);

    int exit_status;
    wait_infinite(child_pid, &exit_status);

    // _exit cleans up the pipes so we don't close them manually
    _exit(exit_status);
  }

  // Only fork needs write endpoints of pipes
  pipe_close(&error_pipe_write);

  error = pipe_read(error_pipe_read, spawn_error, sizeof(*spawn_error), NULL);
  pipe_close(&error_pipe_read);
  if (error) {
    return error;
  }

  *pid = chdir_pid;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR fork_exec_redirect(int argc, const char *argv[],
                                     const char *working_directory,
                                     int stdin_pipe, int stdout_pipe,
                                     int stderr_pipe, pid_t *pid)
{
  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  assert(stdin_pipe);
  assert(stdout_pipe);
  assert(stderr_pipe);
  assert(pid);

  PROCESS_LIB_ERROR error;

  posix_spawnattr_t attributes;
  posix_spawn_file_actions_t actions;
  error = posix_spawn_setup(&attributes, &actions, stdin_pipe, stdout_pipe,
                            stderr_pipe);
  if (error) {
    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&actions);
  }

  int spawn_error = 0;

  // See Design section in readme for how we hack in support for setting the
  // working directory in posix_spawn
  if (!working_directory) {
    spawn_error = posix_spawnp(pid, argv[0], &actions, &attributes,
                               (char **) argv, environ);
  } else {
    error = fork_posix_spawn(argc, argv, working_directory, &attributes,
                             &actions, pid, &spawn_error);
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
    case ENOTDIR: return PROCESS_LIB_FILE_NOT_FOUND;
    case EMFILE: return PROCESS_LIB_PIPE_LIMIT_REACHED;
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    case ENAMETOOLONG: return PROCESS_LIB_NAME_TOO_LONG;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  return PROCESS_LIB_SUCCESS;
}

// See "Avoiding leaking of file descriptors and handles" in the Design chapter
// of the readme for why we use a different implementation of fork_exec_redirect
// on Darwin

#include "fork_exec_redirect.h"

#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

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

  posix_spawnattr_setflags(attributes, flags);
  posix_spawnattr_setpgroup(attributes, 0);

  error = posix_spawn_file_actions_init(actions);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  error = posix_spawn_file_actions_adddup2(actions, stdin_pipe, STDIN_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }
  error = posix_spawn_file_actions_adddup2(actions, stdout_pipe, STDOUT_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }
  error = posix_spawn_file_actions_adddup2(actions, stderr_pipe, STDERR_FILENO);
  if (error) { return PROCESS_LIB_MEMORY_ERROR; }

  return PROCESS_LIB_SUCCESS;
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

    if (chdir(working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // TODO check envp
    errno = posix_spawnp(pid, argv[0], actions, attributes, (char **) argv,
                         environ);
    write(error_pipe_write, &errno, sizeof(errno));
    _exit(errno);
  }

  // Close write end on parent side so read will read 0 if it is closed on the
  // child side as well
  pipe_close(&error_pipe_write);
  ssize_t bytes_read = read(error_pipe_read, &spawn_error, sizeof(spawn_error));
  pipe_close(&error_pipe_read);

  if (waitpid(chdir_pid, NULL, 0) == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

  if (bytes_read == -1) { return PROCESS_LIB_UNKNOWN_ERROR; }

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
    case EINTR: return PROCESS_LIB_INTERRUPTED;
    case ENAMETOOLONG: return PROCESS_LIB_NAME_TOO_LONG;
    }
  }

  return PROCESS_LIB_SUCCESS;
}

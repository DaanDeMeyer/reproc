#include <process.h>

#include "error.h"
#include "macro.h"
#include "pipe.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Including the entire reproc.h header is overkill so we import only the
// constant we need.
extern const unsigned int REPROC_INFINITE;

// Returns true if the null-terminated string indicated by `path` is a relative
// path. A path is relative if any character except the first is a forward slash
// ('/').
static bool path_is_relative(const char *path)
{
  return strlen(path) > 0 && path[0] != '/' && strchr(path + 1, '/') != NULL;
}

// Prepends the null-terminated string indicated by `path` with the current
// working directory. The caller is responsible for freeing the result of this
// function. If an error occurs, `NULL` is returned and `errno` is set to
// indicate the error.
static char *path_prepend_cwd(const char *path)
{
  assert(path);

  size_t path_size = strlen(path);
  size_t cwd_size = PATH_MAX;

  // We always allocate sufficient space for `path` but do not include this
  // space in `cwd_size` so we can be sure that when `getcwd` succeeds there is
  // sufficient space left in `cwd` to append `path`.

  // +2 reserves space to add a null terminator and potentially a missing '/'
  // after the current working directory.
  char *cwd = calloc(cwd_size + path_size + 2, sizeof(char));
  if (cwd == NULL) {
    return cwd;
  }

  while (getcwd(cwd, cwd_size) == NULL) {
    if (errno != ERANGE) {
      free(cwd);
      return NULL;
    }

    cwd_size += PATH_MAX;

    char *result = realloc(cwd, cwd_size + path_size + 1);
    if (result == NULL) {
      free(cwd);
      return result;
    }

    cwd = result;
  }

  cwd_size = strlen(cwd);

  // Add a forward slash after `cwd` if there is none.
  if (cwd[cwd_size - 1] != '/') {
    cwd[cwd_size] = '/';
    cwd[cwd_size + 1] = '\0';
    cwd_size++;
  }

  // We've made sure there's sufficient space left in `cwd` to add `path` and a
  // null terminator.
  memcpy(cwd + cwd_size, path, path_size);
  cwd[cwd_size + path_size] = '\0';

  return cwd;
}

static int write_errno(int fd)
{
  assert(fd != HANDLE_INVALID);
  (void) !write(fd, &errno, sizeof(errno));
  return 0;
}

static pid_t process_fork(int *error_pipe_read,
                          int *error_pipe_write,
                          struct stdio redirect,
                          pid_t group)
{
  assert(error_pipe_read);
  assert(error_pipe_write);
  assert(*error_pipe_read != HANDLE_INVALID);
  assert(*error_pipe_write != HANDLE_INVALID);

  sigset_t old_mask = { 0 };
  sigset_t new_mask = { 0 };
  int r = -1;

  // We don't want signal handlers of the parent to run in the child process so
  // we block all signals before forking.

  r = sigfillset(&new_mask);
  if (r < 0) {
    return error_unify(r);
  }

  r = -pthread_sigmask(SIG_SETMASK, &new_mask, &old_mask);
  if (r < 0) {
    return error_unify(r);
  }

  r = fork();
  if (r < 0) {
    // `fork` error.

    PROTECT_SYSTEM_ERROR;

    r = -pthread_sigmask(SIG_SETMASK, &new_mask, &old_mask);
    assert_unused(r == 0);

    UNPROTECT_SYSTEM_ERROR;

    return error_unify(r);
  }

  if (r > 0) {
    // Parent process

    // From now on, the child process might have started so we don't report
    // errors from `signal_mask` and `pipe_read`. This puts the responsibility
    // for cleaning up the process in the hands of the user from now on.

    PROTECT_SYSTEM_ERROR;

    r = -pthread_sigmask(SIG_SETMASK, &new_mask, &old_mask);
    assert_unused(r == 0);

    // Close the error pipe write end on the parent's side so `read` will return
    // when it is closed on the child side as well.
    *error_pipe_write = handle_destroy(*error_pipe_write);

    int child_errno = 0;
    r = pipe_read(*error_pipe_read, (uint8_t *) &child_errno,
                  sizeof(child_errno));
    assert_unused(r == -EPIPE || r > 0);

    UNPROTECT_SYSTEM_ERROR;

    if (child_errno > 0) {
      r = waitpid(r, NULL, 0);
      if (r >= 0) {
        // If the child writes to the error pipe and exits, we're certain the
        // child process exited on its own and we can report the error as usual.
        r = -child_errno;
      }
    }

    *error_pipe_read = handle_destroy(*error_pipe_read);

    // On success, `r` holds the child process pid.
    return error_unify_or_else(r, r);
  }

  // Child process

  // Reset all signal handlers so they don't run in the child process before
  // `exec` is called. By default, a child process inherits the parent's signal
  // handlers but we override this as most signal handlers won't be written in a
  // way that they can deal with being run in a child process.

  struct sigaction action = { .sa_handler = SIG_DFL };

  r = sigemptyset(&action.sa_mask);
  if (r < 0) {
    goto cleanup;
  }

  for (int i = 0; i < NSIG; i++) {
    r = sigaction(i, &action, NULL);
    if (r < 0 && errno != EINVAL) {
      goto cleanup;
    }
  }

  // Reset the child's signal mask to the default signal mask. By default, a
  // child process inherits the parent's signal mask (even over an `exec` call)
  // but we override this as most processes won't be written in a way that they
  // can deal with starting with a custom signal mask.

  r = sigemptyset(&new_mask);
  if (r < 0) {
    goto cleanup;
  }

  r = -pthread_sigmask(SIG_SETMASK, &new_mask, &old_mask);
  if (r < 0) {
    goto cleanup;
  }

  r = setpgid(0, group);
  if (r < 0) {
    goto cleanup;
  }

  // Redirect stdin, stdout and stderr.

  int vec[3] = { redirect.in, redirect.out, redirect.err };
  int stdio[3] = { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO };

  for (size_t i = 0; i < ARRAY_SIZE(vec); i++) {
    if (vec[i] != HANDLE_INVALID) {
      r = dup2(vec[i], stdio[i]);
      if (r < 0) {
        goto cleanup;
      }
    } else {
      handle_destroy(stdio[i]);
    }
  }

  int max_fd = (int) sysconf(_SC_OPEN_MAX);
  if (max_fd < 0) {
    goto cleanup;
  }

  // Not all file descriptors might have been created with the `FD_CLOEXEC`
  // flag so we manually close all file descriptors (except the standard
  // streams) as an extra measure to prevent file descriptors leaking into the
  // child process.

  for (int i = 3; i < max_fd; i++) {
    // We might still need the error pipe to report `exec` failures so we
    // don't close it. The error pipe has the `FD_CLOEXEC` flag set which
    // guarantees it will be closed automatically when `exec` or `_exit` are
    // called so we don't have to manually close it.
    if (i == *error_pipe_write) {
      continue;
    }

    // Check if `i` is a valid file descriptor before trying to close it.
    r = fcntl(i, F_GETFD);
    if (r >= 0) {
      handle_destroy(i);
    }
  }

  r = 0;

cleanup:
  if (r < 0) {
    _exit(write_errno(*error_pipe_write));
  }

  return 0;
}

static const struct pipe_options PIPE_BLOCKING = { .nonblocking = false };

int process_create(pid_t *process,
                   const char *const *argv,
                   struct process_options options)
{
  assert(process);
  assert(argv);
  assert(argv[0] != NULL);

  int error_pipe_read = HANDLE_INVALID;
  int error_pipe_write = HANDLE_INVALID;
  int r = -1;

  // We create an error pipe to receive errors from the child process.
  r = pipe_init(&error_pipe_read, PIPE_BLOCKING, &error_pipe_write,
                PIPE_BLOCKING);
  if (r < 0) {
    goto cleanup;
  }

  // Put the child process in its own process group so we can use `waitpid` to
  // wait on both the child process and a timeout process at the same time.
  r = process_fork(&error_pipe_read, &error_pipe_write, options.redirect, 0);
  if (r < 0) {
    goto cleanup;
  }

  if (r == 0) {
    // Child process code. Since we're in the child process we can exit on
    // failure.

    // We prepend the parent working directory to `program` if it is a
    // relative path so that it will always be searched for relative to the
    // parent working directory even after executing `chdir`.
    const char *program = options.working_directory && path_is_relative(argv[0])
                              ? path_prepend_cwd(argv[0])
                              : argv[0];
    if (program == NULL) {
      _exit(write_errno(error_pipe_write));
    }

    if (options.working_directory != NULL) {
      r = chdir(options.working_directory);
      if (r < 0) {
        _exit(write_errno(error_pipe_write));
      }
    }

    if (options.environment != NULL) {
      // `execvpe` is not standard POSIX so we overwrite `environ` instead.
      extern char **environ; // NOLINT
      environ = (char **) options.environment;
    }

    execvp(program, (char *const *) argv);

    // We're guaranteed `execvp` failed if this code is reached.
    _exit(write_errno(error_pipe_write));
  }

  *process = r;

cleanup:
  handle_destroy(error_pipe_read);
  handle_destroy(error_pipe_write);

  return error_unify(r);
}

static int parse_status(int status)
{
  return WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status) + UINT8_MAX;
}

int process_wait(pid_t process, unsigned int timeout)
{
  assert(process != HANDLE_INVALID);

  int status = 0;
  int r = -1;

  if (timeout == 0 || timeout == REPROC_INFINITE) {
    r = waitpid(process, &status, timeout == 0 ? WNOHANG : 0);

    if (r >= 0) {
      assert(r == process);
      status = parse_status(status);
    }

    return error_unify_or_else(r, status);
  }

  int error_pipe_read = HANDLE_INVALID;
  int error_pipe_write = HANDLE_INVALID;

  r = pipe_init(&error_pipe_read, PIPE_BLOCKING, &error_pipe_write,
                PIPE_BLOCKING);
  if (r < 0) {
    goto cleanup;
  }

  struct stdio stdio = { HANDLE_INVALID, HANDLE_INVALID, HANDLE_INVALID };

  r = process_fork(&error_pipe_read, &error_pipe_write, stdio, process);
  if (r < 0) {
    goto cleanup;
  }

  if (r == 0) {
    struct timeval tv = {
      .tv_sec = timeout / 1000,          // ms -> s
      .tv_usec = (timeout % 1000) * 1000 // leftover ms -> us
    };

    // Close the error pipe before we sleep so the parent process can continue
    // to the `waitpid` call in the parent process.
    handle_destroy(error_pipe_write);

    // `select` with no file descriptors can be used as a makeshift sleep
    // function that can still be interrupted.
    r = select(0, NULL, NULL, NULL, &tv);

    _exit(r < 0 ? errno : 0);
  }

  pid_t timeout_pid = r;

  r = waitpid(-process, &status, 0);

  if (r != timeout_pid) {
    // We didn't time out. Terminate the timeout process to make sure it exits.
    PROTECT_SYSTEM_ERROR;

    r = process_terminate(timeout_pid);
    assert_unused(r == 0);
    r = waitpid(timeout_pid, NULL, 0);
    assert_unused(r == timeout_pid);

    UNPROTECT_SYSTEM_ERROR;
  }

  if (r < 0) {
    goto cleanup;
  }

  if (r == timeout_pid) {
    // The timeout expired.
    r = -ETIMEDOUT;
    goto cleanup;
  }

  assert(r == process);
  status = parse_status(status);

cleanup:
  handle_destroy(error_pipe_read);
  handle_destroy(error_pipe_write);

  return error_unify_or_else(r, status);
}

int process_terminate(pid_t process)
{
  assert(process != HANDLE_INVALID);

  int r = kill(process, SIGTERM);
  return error_unify(r);
}

int process_kill(pid_t process)
{
  assert(process != HANDLE_INVALID);

  int r = kill(process, SIGKILL);
  return error_unify(r);
}

pid_t process_destroy(pid_t process)
{
  // `waitpid` already cleans up the process for us.
  (void) process;
  return HANDLE_INVALID;
}

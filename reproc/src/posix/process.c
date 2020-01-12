#include "process.h"

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
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static int signal_mask(int how, const sigset_t *newmask, sigset_t *oldmask)
{
  int r = -1;

#if defined(REPROC_MULTITHREADED)
  // `pthread_sigmask` returns positive errno values so we negate them.
  r = -pthread_sigmask(how, newmask, oldmask);
#else
  r = sigprocmask(how, newmask, oldmask);
#endif

  return error_unify(r);
}

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

static const struct pipe_options PIPE_BLOCKING = { .nonblocking = false };
static const int MAX_FD_LIMIT = 1024 * 1024;

static int get_max_fd(void)
{
  struct rlimit limit = { 0 };

  int r = getrlimit(RLIMIT_NOFILE, &limit);
  if (r < 0) {
    return error_unify(r);
  }

  rlim_t soft = limit.rlim_cur;

  if (soft == RLIM_INFINITY || soft > INT_MAX) {
    return INT_MAX;
  }

  return (int) (soft - 1);
}

static bool fd_in_set(int fd, const int *fd_set, size_t size)
{
  for (size_t i = 0; i < size; i++) {
    if (fd == fd_set[i]) {
      return true;
    }
  }

  return false;
}

static pid_t process_fork(const int *except, size_t num_except)
{
  struct {
    sigset_t old;
    sigset_t new;
  } mask;

  int r = -1;

  // We don't want signal handlers of the parent to run in the child process so
  // we block all signals before forking.

  r = sigfillset(&mask.new);
  if (r < 0) {
    return error_unify(r);
  }

  r = signal_mask(SIG_SETMASK, &mask.new, &mask.old);
  if (r < 0) {
    return error_unify(r);
  }

  struct {
    int read;
    int write;
  } pipe = { HANDLE_INVALID, HANDLE_INVALID };

  r = pipe_init(&pipe.read, PIPE_BLOCKING, &pipe.write, PIPE_BLOCKING);
  if (r < 0) {
    return error_unify(r);
  }

  r = fork();
  if (r < 0) {
    // `fork` error.

    PROTECT_SYSTEM_ERROR;

    r = signal_mask(SIG_SETMASK, &mask.new, &mask.old);
    assert_unused(r == 0);

    UNPROTECT_SYSTEM_ERROR;

    handle_destroy(pipe.read);
    handle_destroy(pipe.write);

    return error_unify(r);
  }

  if (r > 0) {
    // Parent process

    pid_t child = r;

    // From now on, the child process might have started so we don't report
    // errors from `signal_mask` and `read`. This puts the responsibility
    // for cleaning up the process in the hands of the caller.

    PROTECT_SYSTEM_ERROR;

    r = signal_mask(SIG_SETMASK, &mask.old, &mask.old);
    assert_unused(r == 0);

    // Close the error pipe write end on the parent's side so `read` will return
    // when it is closed on the child side as well.
    handle_destroy(pipe.write);

    int child_errno = 0;
    r = (int) read(pipe.read, &child_errno, sizeof(child_errno));
    assert_unused(r >= 0);

    UNPROTECT_SYSTEM_ERROR;

    if (child_errno > 0) {
      // If the child writes to the error pipe and exits, we're certain the
      // child process exited on its own and we can report errors as usual.
      r = waitpid(child, NULL, 0);
      assert(r < 0 || r == child);

      if (r == child) {
        r = -child_errno;
      }
    }

    handle_destroy(pipe.read);

    return error_unify_or_else(r, child);
  }

  // Child process

  // Reset all signal handlers so they don't run in the child process. By
  // default, a child process inherits the parent's signal handlers but we
  // override this as most signal handlers won't be written in a way that they
  // can deal with being run in a child process.

  struct sigaction action = { .sa_handler = SIG_DFL };

  r = sigemptyset(&action.sa_mask);
  if (r < 0) {
    goto finish;
  }

  for (int i = 0; i < NSIG; i++) {
    r = sigaction(i, &action, NULL);
    if (r < 0 && errno != EINVAL) {
      goto finish;
    }
  }

  // Reset the child's signal mask to the default signal mask. By default, a
  // child process inherits the parent's signal mask (even over an `exec` call)
  // but we override this as most processes won't be written in a way that they
  // can deal with starting with a custom signal mask.

  r = sigemptyset(&mask.new);
  if (r < 0) {
    goto finish;
  }

  r = signal_mask(SIG_SETMASK, &mask.new, NULL);
  if (r < 0) {
    goto finish;
  }

  // Not all file descriptors might have been created with the `FD_CLOEXEC`
  // flag so we manually close all file descriptors to prevent file descriptors
  // leaking into the child process.

  int max_fd = get_max_fd();
  if (max_fd < 0) {
    goto finish;
  }

  if (max_fd > MAX_FD_LIMIT) {
    // Refuse to try to close too many file descriptors.
    r = -EMFILE;
    goto finish;
  }

  for (int i = 0; i < max_fd; i++) {
    // Make sure we don't close the error pipe file descriptors twice.
    if (i == pipe.read || i == pipe.write) {
      continue;
    }

    if (fd_in_set(i, except, num_except)) {
      continue;
    }

    // Check if `i` is a valid file descriptor before trying to close it.
    r = fcntl(i, F_GETFD);
    if (r >= 0) {
      handle_destroy(i);
    }
  }

  r = 0;

finish:
  if (r < 0) {
    (void) !write(pipe.write, &errno, sizeof(errno));
    _exit(EXIT_FAILURE);
  }

  handle_destroy(pipe.write);
  handle_destroy(pipe.read);

  return 0;
}

int process_start(pid_t *process,
                  const char *const *argv,
                  struct process_options options)
{
  assert(process);

  if (argv != NULL) {
    assert(argv[0] != NULL);
  }

  struct {
    int read;
    int write;
  } pipe = { HANDLE_INVALID, HANDLE_INVALID };
  char *program = NULL;
  int r = -1;

  // We create an error pipe to receive errors from the child process.
  r = pipe_init(&pipe.read, PIPE_BLOCKING, &pipe.write, PIPE_BLOCKING);
  if (r < 0) {
    goto finish;
  }

  if (argv != NULL) {
    // We prepend the parent working directory to `program` if it is a
    // relative path so that it will always be searched for relative to the
    // parent working directory even after executing `chdir`.
    program = options.working_directory && path_is_relative(argv[0])
                  ? path_prepend_cwd(argv[0])
                  : strdup(argv[0]);
    if (program == NULL) {
      r = -1;
      goto finish;
    }
  }

  int except[5] = { options.redirect.in, options.redirect.out,
                    options.redirect.err, pipe.read, pipe.write };

  r = process_fork(except, ARRAY_SIZE(except));
  if (r < 0) {
    goto finish;
  }

  if (r == 0) {
    // Put the process in its own process group which is required by
    // `process_wait`.
    r = setpgid(0, 0);
    if (r < 0) {
      goto child;
    }

    // Redirect stdin, stdout and stderr.

    int redirect[3] = { options.redirect.in, options.redirect.out,
                        options.redirect.err };

    for (size_t i = 0; i < ARRAY_SIZE(redirect); i++) {
      // `i` corresponds to the standard stream we need to redirect.
      r = dup2(redirect[i], (int) i);
      if (r < 0) {
        goto child;
      }
    }

    if (options.working_directory != NULL) {
      r = chdir(options.working_directory);
      if (r < 0) {
        goto child;
      }
    }

    if (options.environment != NULL) {
      // `environ` is carried over calls to `exec`.
      extern char **environ; // NOLINT
      environ = (char **) options.environment;
    }

    if (argv != NULL) {
      assert(program);

      r = execvp(program, (char *const *) argv);
      if (r < 0) {
        goto child;
      }
    }

  child:
    if (r < 0) {
      (void) !write(pipe.write, &errno, sizeof(errno));
      _exit(EXIT_FAILURE);
    }

    handle_destroy(pipe.read);
    handle_destroy(pipe.write);
    free(program);

    return 0;
  }

  pid_t child = r;

  // Close the error pipe write end on the parent's side so `read` will return
  // when it is closed on the child side as well.
  pipe.write = handle_destroy(pipe.write);

  PROTECT_SYSTEM_ERROR;

  int child_errno = 0;
  r = (int) read(pipe.read, &child_errno, sizeof(child_errno));
  assert_unused(r >= 0);

  UNPROTECT_SYSTEM_ERROR;

  if (child_errno > 0) {
    r = waitpid(child, NULL, 0);
    if (r == child) {
      r = -child_errno;
    }

    goto finish;
  }

  *process = r;

finish:
  handle_destroy(pipe.read);
  handle_destroy(pipe.write);
  free(program);

  return error_unify_or_else(r, 1);
}

static int parse_status(int status)
{
  return WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status) + UINT8_MAX;
}

int process_wait(pid_t process, int timeout)
{
  assert(process != HANDLE_INVALID);

  int status = 0;
  int r = -1;

  if (timeout <= 0) {
    r = waitpid(process, &status, timeout == 0 ? WNOHANG : 0);

    if (r > 0) {
      assert(r == process);
      status = parse_status(status);
    }

    if (r == 0) {
      r = -ETIMEDOUT;
    }

    return error_unify_or_else(r, status);
  }

  r = process_fork(NULL, 0);
  if (r < 0) {
    return error_unify(r);
  }

  if (r == 0) {
    struct timeval timeval = {
      .tv_sec = timeout / 1000,          // ms -> s
      .tv_usec = (timeout % 1000) * 1000 // leftover ms -> us
    };

    // `select` with no file descriptors can be used as a makeshift sleep
    // function that can still be interrupted.
    r = select(0, NULL, NULL, NULL, &timeval);

    _exit(r < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  pid_t timeout_pid = r;

  // Put the timeout process in the same process group as the process we're
  // waiting on so we can wait on both at once using `waitpid`. We do this in
  // the parent process to avoid a race condition between calling `setpgid` in
  // the child process and calling `waitpid` in the parent process.
  r = setpgid(timeout_pid, process);

  r = r < 0 ? r : waitpid(-process, &status, 0);

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
    return error_unify(r);
  }

  status = parse_status(status);

  if (r == timeout_pid) {
    // The timeout expired or the timeout process was interrupted.
    return status == 0 ? -ETIMEDOUT : -EINTR;
  }

  assert(r == process);

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

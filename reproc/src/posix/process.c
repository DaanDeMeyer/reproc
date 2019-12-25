#include <process.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/event.h>
#endif

#if defined(__APPLE__)
#define EXECVPE execve
#else
#define EXECVPE execvpe
#endif

// https://github.com/neovim/neovim/pull/5243
#if defined(__APPLE__) && defined(__GNUC__) && !defined(__clang__)
#define SIGADDSET(set, signum) sigaddset((int *) (set), (signum))
#else
#define SIGADDSET(set, signum) sigaddset((set), (signum))
#endif

// Including the entire reproc.h header is overkill so we import only the
// constant we need.
extern const unsigned int REPROC_INFINITE;

#define PROTECT_ERRNO(expression)                                              \
  do {                                                                         \
    int saved_errno = errno;                                                   \
    (void) !(expression);                                                      \
    errno = saved_errno;                                                       \
  } while (false)

static int signal_mask(int how, const sigset_t *newmask, sigset_t *oldmask)
{
#if defined(REPROC_MULTITHREADED)
  errno = pthread_sigmask(how, newmask, oldmask);
  return errno == 0 ? 0 : -1;
#else
  return sigprocmask(how, newmask, oldmask);
#endif
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
  size_t path_size = strlen(path);
  size_t cwd_size = PATH_MAX;

  // We always allocate sufficient space for `path` but do not include this
  // space in `cwd_size` so we can be sure that when `getcwd` succeeds there is
  // sufficient space left in `cwd` to append `path`.

  // +2 reserves space to add a null terminator and potentially a missing '/'
  // after the current working directory.
  char *cwd = calloc(cwd_size + path_size + 2, sizeof(char));
  if (!cwd) {
    return NULL;
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
      return NULL;
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

static int process_pipe(int *read, int *write)
{
  int pipefd[2] = { HANDLE_INVALID, HANDLE_INVALID };
  int r = -1;

#if defined(__APPLE__)
  r = pipe(pipefd);
  if (r < 0) {
    goto cleanup;
  }

  r = fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto cleanup;
  }

  r = fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
  if (r < 0) {
    goto cleanup;
  }
#else
  r = pipe2(pipefd, O_CLOEXEC);
  if (r < 0) {
    goto cleanup;
  }
#endif

  *read = pipefd[0];
  *write = pipefd[1];

cleanup:
  if (r < 0) {
    handle_destroy(pipefd[0]);
    handle_destroy(pipefd[1]);
  }

  return r;
}

static int write_errno(int fd)
{
  PROTECT_ERRNO(write(fd, &errno, sizeof(errno)));
  return errno;
}

static pid_t process_fork(int error_socket)
{
  sigset_t old_mask = { 0 };
  sigset_t new_mask = { 0 };
  int r = -1;

  // We don't want signal handlers of the parent to run in the child process so
  // we block all signals before forking.

  r = sigfillset(&new_mask);
  if (r < 0) {
    return r;
  }

  r = signal_mask(SIG_SETMASK, &new_mask, &old_mask);
  if (r < 0) {
    return r;
  }

  int child = fork();
  if (child != 0) {
    // Parent process or `fork` error.
    PROTECT_ERRNO(signal_mask(SIG_SETMASK, &old_mask, NULL));
    return child;
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

  r = signal_mask(SIG_SETMASK, &new_mask, NULL);
  if (r < 0) {
    goto cleanup;
  }

cleanup:
  if (r < 0) {
    _exit(write_errno(error_socket));
  }

  return r;
}

REPROC_ERROR
process_create(pid_t *process,
               const char *const *argv,
               struct process_options options)
{
  assert(argv);
  assert(argv[0] != NULL);
  assert(process);

  int error_pipe_read = HANDLE_INVALID;
  int error_pipe_write = HANDLE_INVALID;
  int child = HANDLE_INVALID;
  ssize_t r = -1;

  // We create an error socket pair to receive errors from the child process.
  r = process_pipe(&error_pipe_read, &error_pipe_write);
  if (r < 0) {
    goto cleanup;
  }

  r = process_fork(error_pipe_write);
  if (r < 0) {
    goto cleanup;
  }

  if (r == 0) {
    // Child process code. Since we're in the child process we can exit on
    // errors.

    // We prepend the parent working directory to `program` if it is a
    // relative path so that it will always be searched for relative to the
    // parent working directory even after executing `chdir`.
    const char *program = options.working_directory && path_is_relative(argv[0])
                              ? path_prepend_cwd(argv[0])
                              : argv[0];
    if (program == NULL) {
      _exit(write_errno(error_pipe_write));
    }

    if (options.working_directory) {
      r = chdir(options.working_directory);
      if (r < 0) {
        _exit(write_errno(error_pipe_write));
      }
    }

    // Redirect stdin, stdout and stderr.

    r = dup2(options.redirect.in, STDIN_FILENO);
    if (r < 0) {
      _exit(write_errno(error_pipe_write));
    }

    r = dup2(options.redirect.out, STDOUT_FILENO);
    if (r < 0) {
      _exit(write_errno(error_pipe_write));
    }

    r = dup2(options.redirect.err, STDERR_FILENO);
    if (r < 0) {
      _exit(write_errno(error_pipe_write));
    }

    int max_fd = (int) sysconf(_SC_OPEN_MAX);
    if (max_fd < 0) {
      _exit(write_errno(error_pipe_write));
    }

    // Not all file descriptors might have been created with the `FD_CLOEXEC`
    // flag so we manually close all file descriptors (except the standard
    // streams) as an extra measure to prevent file descriptors leaking into the
    // child process.

    for (int i = 3; i < max_fd; i++) {
      // We might still need the error socket to report `exec` failures so we
      // don't close it. The error socket has the `FD_CLOEXEC` flag set which
      // guarantees it will be closed automatically when `exec` or `_exit` are
      // called so we don't have to manually close it.
      if (i == error_pipe_write) {
        continue;
      }

      // We ignore `close` errors since we try to close every file descriptor
      // including invalid ones and `close` sets `errno` when an invalid file
      // descriptor is passed.
      close(i);
    }

    // The casts are safe since `execvp(e)` doesn't actually change the contents
    // of `arguments` and `environment`.
    char *const *arguments = (char *const *) argv;
    char *const *environment = (char *const *) options.environment;

    if (options.environment != NULL) {
      EXECVPE(program, arguments, environment);
    } else {
      execvp(program, arguments);
    }

    // We're guaranteed `execvp(e)` failed if this code is reached.
    _exit(write_errno(error_pipe_write));
  }

  child = (int) r;

  // Close error socket write end on the parent's side so `read` will return
  // when it is closed on the child side as well.
  error_pipe_write = handle_destroy(error_pipe_write);

  // Wait until the child process closes the error socket.
  int child_errno = 0;
  r = read(error_pipe_read, &child_errno, sizeof(child_errno));
  if (r < 0) {
    goto cleanup;
  }

  if (child_errno != 0) {
    // Allow retrieving child process errors with `reproc_error_system`.
    r = -1;
    errno = child_errno;
    goto cleanup;
  }

  *process = child;

cleanup:
  handle_destroy(error_pipe_read);
  handle_destroy(error_pipe_write);

  // Make sure the child process doesn't become a zombie process if the child
  // process was started (`child_process` > 0) but an error occurred.
  if (r < 0 && child > 0) {
    PROTECT_ERRNO(waitpid(child, NULL, 0));
  }

  return r < 0 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

static struct timespec timespec_from_milliseconds(long milliseconds)
{
  struct timespec result;
  result.tv_sec = milliseconds / 1000;              // ms -> s
  result.tv_nsec = (milliseconds % 1000) * 1000000; // leftover ms -> ns

  return result;
}

static struct timespec timespec_subtract(struct timespec lhs,
                                         struct timespec rhs)
{
  struct timespec result = { 0, 0 };

  result.tv_sec = lhs.tv_sec - rhs.tv_sec;
  result.tv_nsec = lhs.tv_nsec - rhs.tv_nsec;

  while (result.tv_nsec < 0) {
    result.tv_nsec += 1000000000;
    result.tv_sec--;
  }

  return result;
}

static int exit_check(pid_t *processes,
                      unsigned int num_processes,
                      unsigned int *completed,
                      int *exit_status)
{
  assert(completed);
  assert(exit_status);

  for (unsigned int i = 0; i < num_processes; i++) {
    int status = 0;
    int r = waitpid(processes[i], &status, WNOHANG);
    if (r < 0) {
      return r;
    }

    if (r > 0) {
      assert(r == processes[i]);
      *completed = i;
      *exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);
      return 0;
    }
  }

  errno = EAGAIN;
  return -1;
}

static int signal_wait(int signal, const struct timespec *timeout)
{
  int r = -1;

#if defined(__APPLE__)
  r = kqueue();
  if (r < 0) {
    return r;
  }

  int queue = r;

  struct kevent event;
  EV_SET(&event, signal, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

  r = kevent(queue, &event, 1, &event, 1, timeout);
  // Translate `kevent` errors into Linux errno style equivalents.
  if (r == 0) {
    r = -1;
    errno = EAGAIN;
  } else if (r > 0 && event.flags & EV_ERROR) {
    r = -1;
    errno = (int) event.data;
  }

  close(queue);

  return r;
#else
  sigset_t set;

  r = SIGADDSET(&set, signal);
  if (r < 0) {
    return r;
  }

  return timeout == NULL ? sigwaitinfo(&set, NULL)
                         : sigtimedwait(&set, NULL, timeout);
#endif
}

REPROC_ERROR process_wait(pid_t *processes,
                          unsigned int num_processes,
                          unsigned int timeout,
                          unsigned int *completed,
                          int *exit_status)
{
  assert(completed);
  assert(exit_status);

  sigset_t chld_mask;
  sigset_t old_mask;
  int r = -1;

  r = sigemptyset(&chld_mask);
  if (r < 0) {
    return REPROC_ERROR_SYSTEM;
  }

  r = SIGADDSET(&chld_mask, SIGCHLD);
  if (r < 0) {
    return REPROC_ERROR_SYSTEM;
  }

  // We block `SIGCHLD` first to avoid a race condition between `exit_check` and
  // `signal_wait` where the child process is still running when `exit_check`
  // returns but finishes before we call `signal_wait`. By blocking `SIGCHLD`
  // before calling `exit_check`, the signal stays pending until `signal_wait`
  // is called which processes the pending signal.
  r = signal_mask(SIG_BLOCK, &chld_mask, &old_mask);
  if (r < 0) {
    return REPROC_ERROR_SYSTEM;
  }

  bool expired = false;
  struct timespec remaining = timespec_from_milliseconds(timeout);

  while (true) {
    // Check if any process in `processes` has finished.

    r = exit_check(processes, num_processes, completed, exit_status);
    if (r >= 0 || errno != EAGAIN) {
      goto cleanup;
    }

    // No process has finished, record the current time so we can calculate how
    // much time has passed after `signal_wait` returns.

    struct timespec before;
    r = clock_gettime(CLOCK_REALTIME, &before);
    if (r < 0) {
      goto cleanup;
    }

    // Wait until a `SIGCHLD` signal occurs or the timeout expires.

    r = signal_wait(SIGCHLD, timeout == REPROC_INFINITE ? NULL : &remaining);
    if (r < 0) {
      if (errno == EAGAIN) {
        r = 0;
        expired = true;
      }

      goto cleanup;
    }

    // A `SIGCHLD` signal occurred, update the remaining time before the timeout
    // expires and repeat the loop.

    struct timespec after;
    r = clock_gettime(CLOCK_REALTIME, &after);
    if (r < 0) {
      goto cleanup;
    }

    struct timespec elapsed = timespec_subtract(after, before);
    remaining = timespec_subtract(remaining, elapsed);
  }

cleanup:
  PROTECT_ERRNO(signal_mask(SIG_SETMASK, &old_mask, NULL));

  return r < 0 ? REPROC_ERROR_SYSTEM
               : expired ? REPROC_ERROR_WAIT_TIMEOUT : REPROC_SUCCESS;
}

REPROC_ERROR process_terminate(pid_t process)
{
  return kill(process, SIGTERM) < 0 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

REPROC_ERROR process_kill(pid_t process)
{
  return kill(process, SIGKILL) < 0 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

pid_t process_destroy(pid_t process)
{
  // `waitpid` already cleans up the process for us.
  (void) process;
  return HANDLE_INVALID;
}

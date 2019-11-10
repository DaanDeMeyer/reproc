#include <process.h>

#include <handle.h>
#include <pipe.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/event.h>
#endif

#if defined(REPROC_MULTITHREADED)
#define SIGMASK pthread_sigmask
#else
#define SIGMASK sigprocmask
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

// Returns true if the null-terminated string indicated by `path` is a relative
// path. A path is relative if any character except the first is a forward slash
// ('/').
static bool path_is_relative(const char *path)
{
  return strlen(path) > 0 && path[0] != '/' && *strchr(path + 1, '/') != '\0';
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
  char *cwd = malloc(sizeof(char) * cwd_size + path_size + 2);
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

static const struct pipe_options PIPE_BLOCKING = { .nonblocking = false };

REPROC_ERROR
process_create(pid_t *process,
               const char *const *argv,
               struct process_options options)
{
  assert(argv);
  assert(argv[0] != NULL);
  assert(process);

  pid_t child_process = 0;
  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  // We create an error pipe to receive errors from the child process. See this
  // answer https://stackoverflow.com/a/1586277 for more information.
  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, PIPE_BLOCKING, &error_pipe_write,
                    PIPE_BLOCKING);
  if (error) {
    goto cleanup;
  }

  child_process = fork();

  if (child_process == 0) {
    // Child process code. Since we're in the child process we can exit on
    // error. Why `_exit`? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    const char *program = argv[0];

    if (options.working_directory) {
      // We prepend the parent working directory to `program` if it is relative
      // so that it will always be searched for relative to the parent working
      // directory even after executing `chdir`.
      if (path_is_relative(program)) {
        // We don't have to free `program` manually as it will be automatically
        // freed when `_exit` or `execvp` is called.
        program = path_prepend_cwd(program);
        if (program == NULL) {
          (void) !write(error_pipe_write, &errno, sizeof(errno));
          _exit(errno);
        }
      }

      if (chdir(options.working_directory) == -1) {
        (void) !write(error_pipe_write, &errno, sizeof(errno));
        _exit(errno);
      }
    }

    int rv = -1;

    // Redirect stdin, stdout and stderr.
    // `_exit` ensures open file descriptors (pipes) are closed.

    rv = dup2(options.redirect.in, STDIN_FILENO);
    if (rv == -1) {
      (void) !write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    rv = dup2(options.redirect.out, STDOUT_FILENO);
    if (rv == -1) {
      (void) !write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    rv = dup2(options.redirect.err, STDERR_FILENO);
    if (rv == -1) {
      (void) !write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    int max_fd = (int) sysconf(_SC_OPEN_MAX);
    if (max_fd == -1) {
      (void) !write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // While using `pipe2` prevents file descriptors created by reproc from
    // leaking into other child processes, file descriptors created outside of
    // reproc without the `FD_CLOEXEC` flag set will still leak into reproc
    // child processes. To mostly get around this after forking and redirecting
    // the standard streams (stdin, stdout, stderr) of the child process we
    // close all file descriptors (except the standard streams) up to
    // `_SC_OPEN_MAX` (obtained with `sysconf`) in the child process.
    // `_SC_OPEN_MAX` describes the maximum number of files that a process can
    // have open at any time. As a result, trying to close every file descriptor
    // up to this number closes all file descriptors of the child process which
    // includes file descriptors that were leaked into the child process.
    // However, an application can manually lower the resource limit at any time
    // (for example with `setrlimit(RLIMIT_NOFILE)`), which can lead to open
    // file descriptors with a value above the new resource limit if they were
    // created before the resource limit was lowered. These file descriptors
    // will not be closed in the child process since only the file descriptors
    // up to the latest resource limit are closed. Of course, this only happens
    // if the application manually lowers the resource limit.

    for (int i = 3; i < max_fd; i++) {
      // We might still need the error pipe so we don't close it. The error pipe
      // is created with `FD_CLOEXEC` which results in it being closed
      // automatically when `exec` or `_exit` are called so we don't have to
      // manually close it.
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

    // We're guaranteed `execvp(e)` failed if this code is executed.
    (void) !write(error_pipe_write, &errno, sizeof(errno));
    _exit(errno);
  }

  if (child_process == -1) {
    error = REPROC_ERROR_SYSTEM;
    goto cleanup;
  }

  // Close error pipe write end on the parent's side so `pipe_read` will return
  // when it is closed on the child side as well.
  handle_close(&error_pipe_write);

  int child_error = 0;
  unsigned int bytes_read = 0;

  // `pipe_read` blocks until an error is reported from the child process or the
  // write end of the error pipe in the child process is closed.
  error = pipe_read(error_pipe_read, (uint8_t *) &child_error,
                    sizeof(child_error), &bytes_read);
  handle_close(&error_pipe_read);

  switch (error) {
    // `REPROC_ERROR_STREAM_CLOSED` is not an error because it means the pipe
    // was closed without an error being written to it.
    case REPROC_SUCCESS:
    case REPROC_ERROR_STREAM_CLOSED:
      break;
    default:
      goto cleanup;
  }

  // If an error was written to the error pipe we check that a full integer was
  // actually read. We don't expect a partial write to happen but if it ever
  // happens this should make it easier to discover.
  assert(error == REPROC_ERROR_STREAM_CLOSED ||
         bytes_read == sizeof(child_error));

  // If `read` does not return 0 an error will have occurred in the child
  // process (or with `read` itself but this is less likely).
  if (child_error != 0) {
    // Allow retrieving child process errors with `reproc_error_system`.
    error = REPROC_ERROR_SYSTEM;
    errno = child_error;
    goto cleanup;
  }

  error = REPROC_SUCCESS;
  *process = child_process;

cleanup:
  handle_close(&error_pipe_read);
  handle_close(&error_pipe_write);

  // Make sure the child process doesn't become a zombie process if the child
  // process was started (`child_process` > 0) but an error occurred.
  if (error && child_process > 0 && waitpid(child_process, NULL, 0) == -1) {
    error = REPROC_ERROR_SYSTEM;
  }

  return error;
}

static unsigned int parse_exit_status(int status)
{
  // `WEXITSTATUS` returns a value between [0,256) so casting to `unsigned int`
  // is safe.
  if (WIFEXITED(status)) {
    return (unsigned int) WEXITSTATUS(status);
  }

  assert(WIFSIGNALED(status));

  return (unsigned int) WTERMSIG(status);
}

static REPROC_ERROR wait_no_hang(pid_t process, unsigned int *exit_status)
{
  assert(exit_status);

  int status = 0;
  // Adding `WNOHANG` makes `waitpid` only check if the child process is still
  // running without waiting.
  pid_t rv = waitpid(process, &status, WNOHANG);
  if (rv == 0) {
    return REPROC_ERROR_WAIT_TIMEOUT;
  } else if (rv == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  assert(rv == process);
  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}

static REPROC_ERROR wait_infinite(pid_t process, unsigned int *exit_status)
{
  assert(exit_status);

  int status = 0;

  pid_t rv = waitpid(process, &status, 0);
  if (rv == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  assert(rv == process);
  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
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

static REPROC_ERROR
wait_timeout(pid_t process, unsigned int timeout, unsigned int *exit_status)
{
  assert(timeout > 0);
  assert(exit_status);

  sigset_t chld_mask;
  sigset_t old_mask;

  sigemptyset(&chld_mask);
  SIGADDSET(&chld_mask, SIGCHLD);

  // We block `SIGCHLD` to avoid a race condition between `wait_no_hang` and
  // `sigtimedwait` where the child process is still running when `wait_no_hang`
  // returns but exits before we call `sigtimedwait`. By blocking the signal, it
  // stays pending when we receive the signal and since `sigtimedwait` watches
  // for pending signals the race condition is solved.
  if (SIGMASK(SIG_BLOCK, &chld_mask, &old_mask) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  int status = 0;

  struct timespec remaining;
  remaining.tv_sec = timeout / 1000;              // ms -> s
  remaining.tv_nsec = (timeout % 1000) * 1000000; // leftover ms -> ns

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  // MacOS does not support `sigtimedwait` so we use `kqueue` instead.
#if defined(__APPLE__)
  int queue = kqueue();
  struct kevent event;
  EV_SET(&event, SIGCHLD, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
  kevent(queue, &event, 1, NULL, 0, NULL);
#endif

  while (true) {
    int rv = waitpid(process, &status, WNOHANG);
    if (rv == -1) {
      goto cleanup;
    } else if (rv > 0) {
      break;
    }

    struct timespec before;
    rv = clock_gettime(CLOCK_REALTIME, &before);
    if (rv == -1) {
      goto cleanup;
    }

#if defined(__APPLE__)
    rv = kevent(queue, NULL, 0, &event, 1, &remaining);

    // Translate `kevent` errors to their `sigtimedwait` equivalents.
    if (rv == 0) {
      rv = -1;
      errno = EAGAIN;
    } else if (rv > 0 && event.flags & EV_ERROR) {
      rv = -1;
      errno = (int) event.data;
    }
#else
    rv = sigtimedwait(&chld_mask, NULL, &remaining);
#endif

    // Check if the timeout has expired.
    if (rv == -1 && errno == EAGAIN) {
      error = REPROC_ERROR_WAIT_TIMEOUT;
      goto cleanup;
    }

    // Anything other than `EINTR` means system failure. If `EINTR` occurs or we
    // get a `SIGCHLD`, we subtract the passed time from `remaining` and try
    // again. Note that a `SIGCHLD` doesn't necessarily mean that the child
    // process we're waiting for has exited so we keep looping until the timeout
    // expires or `waitpid` verifies that the process we're waiting for has
    // stopped.

    if (rv == -1 && errno != EINTR) {
      goto cleanup;
    }

    struct timespec after;
    rv = clock_gettime(CLOCK_REALTIME, &after);
    if (rv == -1) {
      goto cleanup;
    }

    struct timespec passed = timespec_subtract(after, before);
    remaining = timespec_subtract(remaining, passed);

    // Avoid passing a negative timeout to `sigtimedwait`.
    if (remaining.tv_sec < 0) {
      remaining.tv_sec = 0;
      remaining.tv_nsec = 0;
    }
  }

  error = REPROC_SUCCESS;
  *exit_status = parse_exit_status(status);

cleanup:
#if defined(__APPLE__)
  close(queue);
#endif

  if (SIGMASK(SIG_SETMASK, &old_mask, NULL) == -1) {
    error = REPROC_ERROR_SYSTEM;
  }

  return error;
}

REPROC_ERROR
process_wait(pid_t process, unsigned int timeout, unsigned int *exit_status)
{
  assert(exit_status);

  if (timeout == 0) {
    return wait_no_hang(process, exit_status);
  }

  if (timeout == REPROC_INFINITE) {
    return wait_infinite(process, exit_status);
  }

  return wait_timeout(process, timeout, exit_status);
}

REPROC_ERROR process_terminate(pid_t process)
{
  if (kill(process, SIGTERM) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_kill(pid_t process)
{
  if (kill(process, SIGKILL) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

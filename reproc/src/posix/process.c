#include "process.h"

#include "fd.h"
#include "path.h"
#include "pipe.h"

#include <reproc/config.h>
#include <reproc/reproc.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/event.h>
#endif

#if defined(REPROC_MULTITHREADED)
#define SIGMASK_SAFE pthread_sigmask
#else
#define SIGMASK_SAFE sigprocmask
#endif

#if defined(__APPLE__)
#define EXECVPE execve
#else
#define EXECVPE execvpe
#endif

REPROC_ERROR
process_create(const char *const *argv,
               struct process_options *options,
               pid_t *pid)
{
  assert(argv);
  assert(argv[0] != NULL);
  assert(pid);

  // Predeclare variables so we can use `goto`.
  REPROC_ERROR error = REPROC_SUCCESS;
  pid_t child_pid = 0;
  int child_error = 0;
  unsigned int bytes_read = 0;

  // We create an error pipe to receive errors from the child process. See this
  // answer https://stackoverflow.com/a/1586277 for more information.
  int error_pipe_read = 0;
  int error_pipe_write = 0;
  error = pipe_init(&error_pipe_read, &error_pipe_write);
  if (error) {
    goto cleanup;
  }

  child_pid = fork();

  if (child_pid == 0) {
    // Child process code. Since we're in the child process we can exit on
    // error. Why `_exit`? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    const char *program = argv[0];

    if (options->working_directory) {
      // We prepend the parent working directory to `program` if it is relative
      // so that it will always be searched for relative to the parent working
      // directory even after executing `chdir`.
      if (path_is_relative(program)) {
        // We don't have to free `program` manually as it will be automatically
        // freed when `_exit` or `execvp` is called.
        program = path_prepend_cwd(program);
        if (program == NULL) {
          write(error_pipe_write, &errno, sizeof(errno));
          _exit(errno);
        }
      }

      if (chdir(options->working_directory) == -1) {
        write(error_pipe_write, &errno, sizeof(errno));
        _exit(errno);
      }
    }

    // Redirect stdin, stdout and stderr if required.
    // `_exit` ensures open file descriptors (pipes) are closed.

    if (options->stdin_fd && dup2(options->stdin_fd, STDIN_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (options->stdout_fd && dup2(options->stdout_fd, STDOUT_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (options->stderr_fd && dup2(options->stderr_fd, STDERR_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    int max_fd = (int) sysconf(_SC_OPEN_MAX);
    if (max_fd == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // Close open file descriptors in the child process.
    for (int i = 3; i < max_fd; i++) {
      // We might still need the error pipe so we don't close it. The error pipe
      // is created with `FD_CLOEXEC` which results in it being closed
      // automatically when `exec` or `_exit` are called so we don't have to
      // manually close it.
      if (i == error_pipe_write) {
        continue;
      }

      close(i);
    }

    // Ignore `close` errors since we try to close every file descriptor and
    // `close` sets `errno` when an invalid file descriptor is passed.

    // Replace the forked process with the process specified in `argv`'s first
    // element. The casts are safe since `execvpe` doesn't actually change the
    // contents of `argv` and `envp`.

    int err = 0;

    if (options->environment != NULL) {
      err = EXECVPE(program, (char *const *) argv,
                    (char *const *) options->environment);
    } else {
      err = execvp(program, (char *const *) argv);
    }

    if (err == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
    }

    _exit(errno);
  }

  if (child_pid == -1) {
    error = REPROC_ERROR_SYSTEM;
    goto cleanup;
  }

  // Close error pipe write end on the parent's side so `pipe_read` will return
  // when it is closed on the child side as well.
  fd_close(&error_pipe_write);

  // `pipe_read` blocks until an error is reported from the child process or the
  // write end of the error pipe in the child process is closed.
  error = pipe_read(error_pipe_read, (uint8_t *) &child_error,
                    sizeof(child_error), &bytes_read);
  fd_close(&error_pipe_read);

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
  if (error == REPROC_SUCCESS && bytes_read != sizeof(child_error)) {
    error = REPROC_ERROR_SYSTEM;
    goto cleanup;
  }

  // If `read` does not return 0 an error will have occurred in the child
  // process (or with `read` itself but this is less likely).
  if (child_error != 0) {
    // Allow retrieving child process errors with `reproc_system_error`.
    errno = child_error;
    error = REPROC_ERROR_SYSTEM;
    goto cleanup;
  }

cleanup:
  fd_close(&error_pipe_read);
  fd_close(&error_pipe_write);

  // `REPROC_ERROR_STREAM_CLOSED` is not an error here (see above).
  if (error != REPROC_SUCCESS && error != REPROC_ERROR_STREAM_CLOSED &&
      child_pid > 0) {
    // Make sure the child process doesn't become a zombie process the child
    // process was started (`child_pid` > 0) but an error occurred.
    if (waitpid(child_pid, NULL, 0) == -1) {
      return REPROC_ERROR_SYSTEM;
    }

    return error;
  }

  *pid = child_pid;

  return REPROC_SUCCESS;
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

static REPROC_ERROR wait_no_hang(pid_t pid, unsigned int *exit_status)
{
  assert(exit_status);

  int status = 0;
  // Adding `WNOHANG` makes `waitpid` only check if the child process is still
  // running without waiting.
  pid_t wait_result = waitpid(pid, &status, WNOHANG);
  if (wait_result == 0) {
    return REPROC_ERROR_WAIT_TIMEOUT;
  } else if (wait_result == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}

static REPROC_ERROR wait_infinite(pid_t pid, unsigned int *exit_status)
{
  assert(exit_status);

  int status = 0;

  if (waitpid(pid, &status, 0) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}

static struct timespec timespec_subtract(struct timespec lhs,
                                         struct timespec rhs)
{
  struct timespec result = { 0 };

  result.tv_sec = lhs.tv_sec - rhs.tv_sec;
  result.tv_nsec = lhs.tv_nsec - rhs.tv_nsec;

  while (result.tv_nsec < 0) {
    result.tv_nsec += 1000000000;
    result.tv_sec--;
  }

  return result;
}

static REPROC_ERROR
wait_timeout(pid_t pid, unsigned int timeout, unsigned int *exit_status)
{
  assert(timeout > 0);
  assert(exit_status);

  REPROC_ERROR error = REPROC_ERROR_SYSTEM;

  struct timespec remaining;
  remaining.tv_sec = timeout / 1000;              // ms -> s
  remaining.tv_nsec = (timeout % 1000) * 1000000; // leftover ms -> ns

  sigset_t chld_mask;
  sigset_t old_mask;

  sigemptyset(&chld_mask);
  sigaddset(&chld_mask, SIGCHLD);

  // We block `SIGCHLD` to avoid a race condition between `wait_no_hang` and
  // `sigtimedwait` where the child process is still running when `wait_no_hang`
  // returns but exits before we call `sigtimedwait`. By blocking the signal, it
  // stays pending when we receive the signal and since `sigtimedwait` watches
  // for pending signals the race condition is solved.
  if (SIGMASK_SAFE(SIG_BLOCK, &chld_mask, &old_mask) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  // MacOS does not support `sigtimedwait` so we use `kqueue` instead.
#if defined(__APPLE__)
  int queue = kqueue();
  struct kevent event;
  EV_SET(&event, SIGCHLD, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
  kevent(queue, &event, 1, NULL, 0, NULL);
#endif

  while (true) {
    error = wait_no_hang(pid, exit_status);
    if (error != REPROC_ERROR_WAIT_TIMEOUT) {
      goto cleanup;
    }

    struct timespec before;
    int rv = clock_gettime(CLOCK_REALTIME, &before);
    if (rv == -1) {
      error = REPROC_ERROR_SYSTEM;
      goto cleanup;
    }

#if defined(__APPLE__)
    rv = kevent(queue, NULL, 0, &event, 1, &remaining);

    // Convert `kqueue` timeout error to `sigtimedwait` timeout error.
    if (rv == 0) {
      rv = -1;
      errno = EAGAIN;
    }

    // Translate errors put into the kqueue event list to `rv` and `errno`
    // errors.
    if (rv > 0 && event.flags & EV_ERROR) {
      rv = -1;
      errno = (int) event.data;
    }
#else
    rv = sigtimedwait(&chld_mask, NULL, &remaining);
#endif

    // Check if timeout has expired.
    if (rv == -1 && errno == EAGAIN) {
      error = REPROC_ERROR_WAIT_TIMEOUT;
      break;
    }

    // Anything other than `EINTR` means system failure. If `EINTR` occurs or we
    // get a `SIGCHLD`, we subtract the passed time from `remaining` and try
    // again. Note that a `SIGCHLD` doesn't necessarily mean that the child
    // process we're waiting for has exited so we keep looping until the timeout
    // expires or `wait_no_hang` returns something different then
    // `REPROC_ERROR_WAIT_TIMEOUT`.

    if (rv == -1 && errno != EINTR) {
      error = REPROC_ERROR_SYSTEM;
      goto cleanup;
    }

    struct timespec after;
    rv = clock_gettime(CLOCK_REALTIME, &after);
    if (rv == -1) {
      error = REPROC_ERROR_SYSTEM;
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

cleanup:
#if defined(__APPLE__)
  close(queue);
#endif

  if (SIGMASK_SAFE(SIG_SETMASK, &old_mask, NULL) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  return error;
}

REPROC_ERROR
process_wait(pid_t pid, unsigned int timeout, unsigned int *exit_status)
{
  assert(exit_status);

  if (timeout == 0) {
    return wait_no_hang(pid, exit_status);
  }

  if (timeout == REPROC_INFINITE) {
    return wait_infinite(pid, exit_status);
  }

  return wait_timeout(pid, timeout, exit_status);
}

REPROC_ERROR process_terminate(pid_t pid)
{
  if (kill(pid, SIGTERM) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_kill(pid_t pid)
{
  if (kill(pid, SIGKILL) == -1) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

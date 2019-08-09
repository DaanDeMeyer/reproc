#include "process.h"

#include "fd.h"
#include "pipe.h"

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

// Disable address sanitizers for `process_create` to avoid a false positive
// when using `vfork` and `sigprocmask` simultaneously.
#if !defined(REPROC_MULTITHREADED) && defined(ADDRESS_SANITIZERS)
__attribute__((no_sanitize("address")))
#endif
REPROC_ERROR
process_create(const char *const *argv,
               const char *working_directory,
               int stdin_fd,
               int stdout_fd,
               int stderr_fd,
               pid_t *pid)
{
  assert(stdin_fd >= 0);
  assert(stdout_fd >= 0);
  assert(stderr_fd >= 0);
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

  // `sysconf` is not signal safe so we retrieve the fd limit before forking.
  // This results in the same value as calling it in the child since the child
  // inherits the parent's resource limits.
  int max_fd = (int) sysconf(_SC_OPEN_MAX);
  if (max_fd == -1) {
    error = REPROC_ERROR_SYSTEM;
    goto cleanup;
  }

  // The code inside this block is based on code written by a Redhat employee.
  // The original code along with detailed comments can be found here:
  // https://bugzilla.redhat.com/attachment.cgi?id=941229.
  //
  // Copyright (c) 2014 Red Hat Inc.
  //
  // Written by Carlos O'Donell <codonell@redhat.com>
  //
  // Permission is hereby granted, free of charge, to any person obtaining a
  // copy of this software and associated documentation files (the
  // "Software"), to deal in the Software without restriction, including
  // without limitation the rights to use, copy, modify, merge, publish,
  // distribute, sublicense, and/or sell copies of the Software, and to permit
  // persons to whom the Software is furnished to do so, subject to the
  // following conditions:
  //
  // The above copyright notice and this permission notice shall be included
  // in all copies or substantial portions of the Software.
  //
  // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  // OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  // MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
  // NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  // DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
  // OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
  // USE OR OTHER DEALINGS IN THE SOFTWARE.

  sigset_t all_blocked;
  sigset_t old_mask;

  // Block all signals in the parent before calling vfork. This is for the
  // safety of the child which inherits signal dispositions and handlers. The
  // child, running in the parent's stack, may be delivered a signal. For
  // example on Linux a killpg call delivering a signal to a process group may
  // deliver the signal to the vfork-ing child and you want to avoid this. The
  // easy way to do this is via: sigemptyset, sigaction, and then undo this
  // when you return to the parent. To be completely correct the child should
  // set all non-SIG_IGN signals to SIG_DFL and the restore the original
  // signal mask, thus allowing the vforking child to receive signals that
  // were actually intended for it, but without executing any handlers the
  // parent had setup that could corrupt state. When using glibc and Linux
  // these functions i.e. sigemtpyset, sigaction, etc. are safe to use after
  // vfork.

  if (sigfillset(&all_blocked) == -1) {
    error = REPROC_ERROR_SYSTEM;
    goto cleanup;
  }

  if (SIGMASK_SAFE(SIG_BLOCK, &all_blocked, &old_mask) == -1) {
    error = REPROC_ERROR_SYSTEM;
    goto cleanup;
  }

  child_pid = vfork(); // NOLINT

  if (child_pid == 0) {
    // `vfork` succeeded and we're in the child process. This block contains
    // all `vfork` specific child process code.

    // We reset all signal dispositions that aren't SIG_IGN to SIG_DFL. This
    // is done because the child may have a legitimate need to receive a
    // signal and the default actions should be taken for those signals. Those
    // default actions will not corrupt state in the parent.

    sigset_t empty_mask;

    if (sigemptyset(&empty_mask) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    struct sigaction old_sa;
    struct sigaction new_sa = { .sa_handler = SIG_DFL, .sa_mask = empty_mask };

    for (int i = 0; i < NSIG; i++) {
      // Continue if the signal does not exist, is ignored or is already set
      // to the default signal handler.
      if (sigaction(i, NULL, &old_sa) == -1 || old_sa.sa_handler == SIG_IGN ||
          old_sa.sa_handler == SIG_DFL) {
        continue;
      }

      // POSIX says it is unspecified whether an attempt to set the action for
      // a signal that cannot be caught or ignored to SIG_DFL is ignored or
      // causes an error to be returned with errno set to [EINVAL].

      // Ignore errors if it's EINVAL since those are likely signals we can't
      // change.
      if (sigaction(i, &new_sa, NULL) == -1 && errno != EINVAL) {
        write(error_pipe_write, &errno, sizeof(errno));
        _exit(errno);
      }
    }

    // Restore the old signal mask from the parent process.
    if (SIGMASK_SAFE(SIG_SETMASK, &old_mask, NULL) != 0) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // At this point we can carry out anything else we need to do before exec
    // like changing directory etc.  Signals are enabled in the child and will
    // do their default actions, and the parent's handlers do not run. The
    // caller has ensured not to call set*id functions. The only remaining
    // general restriction is not to corrupt the parent's state by calling
    // complex functions (the safe functions should be documented by glibc but
    // aren't).

  } else {
    // In the parent process we restore the old signal mask regardless of
    // whether `vfork` succeeded or not.
    if (SIGMASK_SAFE(SIG_SETMASK, &old_mask, NULL) != 0) {
      error = REPROC_ERROR_SYSTEM;
      goto cleanup;
    }
  }

  // The rest of the code is identical regardless of whether `fork` or `vfork`
  // was used.

  if (child_pid == 0) {
    // Child process code. Since we're in the child process we can exit on
    // error. Why `_exit`? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    if (working_directory && chdir(working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    // Redirect stdin, stdout and stderr if required.
    // `_exit` ensures open file descriptors (pipes) are closed.

    if (stdin_fd && dup2(stdin_fd, STDIN_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (stdout_fd && dup2(stdout_fd, STDOUT_FILENO) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }
    if (stderr_fd && dup2(stderr_fd, STDERR_FILENO) == -1) {
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
    // element. The cast is safe since `execvp` doesn't actually change the
    // contents of `argv`.
    if (execvp(argv[0], (char **) argv) == -1) {
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
  int kq = kqueue();
  struct kevent ke;
  EV_SET(&ke, SIGCHLD, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
  kevent(kq, &ke, 1, NULL, 0, NULL);
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
    rv = kevent(kq, NULL, 0, &ke, 1, &remaining);

    // Convert `kqueue` timeout to `sigtimedwait` timeout.
    if (rv == 0) {
      rv = -1;
      errno = EAGAIN;
    }

    // Translate errors put into the event list to `rv` and `errno`.
    if (rv > 0 && ke.flags & EV_ERROR) {
      rv = -1;
      errno = (int) ke.data;
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
  close(kq);
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

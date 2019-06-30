#include "process.h"

#include "fd.h"
#include "pipe.h"

#include <reproc/reproc.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

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
process_create(int (*action)(const void *),
               const void *context,
               struct process_options *options,
               pid_t *pid)
{
  assert(options->stdin_fd >= 0);
  assert(options->stdout_fd >= 0);
  assert(options->stderr_fd >= 0);
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
    error = REPROC_UNKNOWN_ERROR;
    goto cleanup;
  }

  if (options->vfork) {
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
      error = REPROC_UNKNOWN_ERROR;
      goto cleanup;
    }

    if (SIGMASK_SAFE(SIG_BLOCK, &all_blocked, &old_mask) == -1) {
      error = REPROC_UNKNOWN_ERROR;
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
      struct sigaction new_sa = { .sa_handler = SIG_DFL,
                                  .sa_mask = empty_mask };

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
        error = REPROC_UNKNOWN_ERROR;
        goto cleanup;
      }
    }
  } else {
    child_pid = fork();
  }

  // The rest of the code is identical regardless of whether `fork` or `vfork`
  // was used.

  if (child_pid == 0) {
    // Child process code. Since we're in the child process we can exit on
    // error. Why `_exit`? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    /* Normally there might be a race condition if the parent process waits for
    the child process before the child process puts itself in its own process
    group (using `setpgid`) but this is avoided because we always read from the
    error pipe in the parent process after forking. When `read` returns the
    child process will either have returned an error (and waiting won't be
    valid) or will have executed `_exit` or `exec` (and as a result `setpgid`
    as well). */
    if (setpgid(0, options->process_group) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
    }

    if (options->working_directory && chdir(options->working_directory) == -1) {
      write(error_pipe_write, &errno, sizeof(errno));
      _exit(errno);
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

    // Closing the error pipe write end will unblock the `pipe_read` call in the
    // parent process which allows it to continue executing.
    if (options->return_early) {
      fd_close(&error_pipe_write);
    }

    // Finally, call the makeshift lambda provided by the caller with the
    // accompanying context object.
    int action_error = action(context);

    // If we didn't return early the error pipe write end is still open and we
    // can use it to report an optional error from action.
    if (!options->return_early) {
      write(error_pipe_write, &action_error, sizeof(action_error));
    }

    _exit(action_error);
  }

  if (child_pid == -1) {
    switch (errno) {
      case EAGAIN:
        error = REPROC_PROCESS_LIMIT_REACHED;
        break;
      case ENOMEM:
        error = REPROC_NOT_ENOUGH_MEMORY;
        break;
      default:
        error = REPROC_UNKNOWN_ERROR;
        break;
    }

    goto cleanup;
  }

  // Close error pipe write end on the parent's side so `pipe_read` will return
  // when it is closed on the child side as well.
  fd_close(&error_pipe_write);

  // `pipe_read` blocks until an error is reported from the child process or the
  // write end of the error pipe in the child process is closed.
  error = pipe_read(error_pipe_read, &child_error, sizeof(child_error),
                    &bytes_read);
  fd_close(&error_pipe_read);

  switch (error) {
    case REPROC_SUCCESS:
      break;
    // `REPROC_STREAM_CLOSED` is not an error because it means the pipe was
    // closed without an error being written to it.
    case REPROC_STREAM_CLOSED:
      break;
    default:
      goto cleanup;
  }

  // If an error was written to the error pipe we check that a full integer was
  // actually read. We don't expect a partial write to happen but if it ever
  // happens this should make it easier to discover.
  if (error == REPROC_SUCCESS && bytes_read != sizeof(child_error)) {
    error = REPROC_UNKNOWN_ERROR;
    goto cleanup;
  }

  // If `read` does not return 0 an error will have occurred in the child
  // process (or with `read` itself but this is less likely).
  if (child_error != 0) {
    // Allow retrieving child process errors with `reproc_system_error`.
    errno = child_error;

    switch (child_error) {
      case EACCES:
        error = REPROC_PERMISSION_DENIED;
        break;
      case EPERM:
        error = REPROC_PERMISSION_DENIED;
        break;
      case ELOOP:
        error = REPROC_SYMLINK_LOOP;
        break;
      case ENAMETOOLONG:
        error = REPROC_NAME_TOO_LONG;
        break;
      case ENOENT:
        error = REPROC_FILE_NOT_FOUND;
        break;
      case ENOTDIR:
        error = REPROC_FILE_NOT_FOUND;
        break;
      case EINTR:
        error = REPROC_INTERRUPTED;
        break;
      default:
        error = REPROC_UNKNOWN_ERROR;
        break;
    }

    goto cleanup;
  }

cleanup:
  fd_close(&error_pipe_read);
  fd_close(&error_pipe_write);

  // `REPROC_STREAM_CLOSED` is not an error here (see above).
  if (error != REPROC_SUCCESS && error != REPROC_STREAM_CLOSED &&
      child_pid > 0) {
    // Make sure the child process doesn't become a zombie process the child
    // process was started (`child_pid` > 0) but an error occurred.
    if (waitpid(child_pid, NULL, 0) == -1) {
      return REPROC_UNKNOWN_ERROR;
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
    return REPROC_WAIT_TIMEOUT;
  } else if (wait_result == -1) {
    // Ignore `EINTR`, it shouldn't happen when using `WNOHANG`.
    return REPROC_UNKNOWN_ERROR;
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}

static REPROC_ERROR wait_infinite(pid_t pid, unsigned int *exit_status)
{
  assert(exit_status);

  int status = 0;

  if (waitpid(pid, &status, 0) == -1) {
    switch (errno) {
      case EINTR:
        return REPROC_INTERRUPTED;
      default:
        return REPROC_UNKNOWN_ERROR;
    }
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
}

// Makeshift C lambda which is passed to `process_create`.
static int timeout_process(const void *context)
{
  unsigned int milliseconds = *((const unsigned int *) context);

  struct timeval tv;
  tv.tv_sec = milliseconds / 1000;           // ms -> s
  tv.tv_usec = (milliseconds % 1000) * 1000; // leftover ms -> us

  // `select` with no file descriptors can be used as a makeshift sleep function
  // that can still be interrupted.
  if (select(0, NULL, NULL, NULL, &tv) == -1) {
    return errno;
  }

  return 0;
}

static REPROC_ERROR timeout_map_error(int error)
{
  switch (error) {
    case EINTR:
      return REPROC_INTERRUPTED;
    case ENOMEM:
      return REPROC_NOT_ENOUGH_MEMORY;
    default:
      return REPROC_UNKNOWN_ERROR;
  }
}

static REPROC_ERROR
wait_timeout(pid_t pid, unsigned int timeout, unsigned int *exit_status)
{
  assert(timeout > 0);
  assert(exit_status);

  REPROC_ERROR error = REPROC_SUCCESS;

  // Check if the child process has already exited before starting a
  // possibly expensive timeout process. If `wait_no_hang` doesn't time out we
  // can return early.
  error = wait_no_hang(pid, exit_status);
  if (error != REPROC_WAIT_TIMEOUT) {
    return error;
  }

  struct process_options options = {
    // `waitpid` supports waiting for the first process that exits in a process
    // group. To take advantage of this we put the timeout process in the same
    // process group as the process we're waiting for.
    .process_group = pid,
    // Return early so `process_create` doesn't block until the timeout has
    // expired.
    .return_early = true,
    // Don't `vfork` because when `vfork` is used the parent process is
    // suspended until the child process calls `exec` or `_exit`.
    // `timeout_process` doesn't call either which results in the parent process
    // being suspended until the timeout process exits. This prevents the parent
    // process from waiting until either the child process or the timeout
    // process expires (which is what we need to do) so we don't use `vfork` to
    // avoid this.
    .vfork = false
  };

  pid_t timeout_pid = 0;
  error = process_create(timeout_process, &timeout, &options, &timeout_pid);
  if (error == REPROC_UNKNOWN_ERROR) {
    error = timeout_map_error(errno);
  }

  if (error) {
    return error;
  }

  // Passing `-reproc->pid` to `waitpid` makes it wait for the first process in
  // the `reproc->pid` process group to exit. The `reproc->pid` process group
  // consists out of the child process we're waiting for and the timeout
  // process. As a result, calling `waitpid` on the `reproc->pid` process group
  // translates to waiting for either the child process or the timeout process
  // to exit.
  int status = 0;
  pid_t exit_pid = waitpid(-pid, &status, 0);

  // If the timeout process exits first the timeout will have expired.
  if (exit_pid == timeout_pid) {
    return REPROC_WAIT_TIMEOUT;
  }

  // If the child process exits first we clean up the timeout process.
  error = process_terminate(timeout_pid);
  if (error) {
    return error;
  }

  unsigned int timeout_exit_status = 0;
  error = wait_infinite(timeout_pid, &timeout_exit_status);
  if (error) {
    return error;
  }

  if (timeout_exit_status != 0 && timeout_exit_status != SIGTERM) {
    errno = (int) timeout_exit_status;
    return REPROC_UNKNOWN_ERROR;
  }

  // After cleaning up the timeout process we can check if `waitpid` returned an
  // error.
  if (exit_pid == -1) {
    switch (errno) {
      case EINTR:
        return REPROC_INTERRUPTED;
      default:
        return REPROC_UNKNOWN_ERROR;
    }
  }

  *exit_status = parse_exit_status(status);

  return REPROC_SUCCESS;
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
    return REPROC_UNKNOWN_ERROR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_kill(pid_t pid)
{
  if (kill(pid, SIGKILL) == -1) {
    return REPROC_UNKNOWN_ERROR;
  }

  return REPROC_SUCCESS;
}

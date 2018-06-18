#if defined(__unix__) || defined(__APPLE__)

#include "process.hpp"

#include <array>
#include <climits>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

struct process::impl {
  // Parent pipe endpoint fd's (write, read, read)
  int stdin = 0;
  int stdout = 0;
  int stderr = 0;
  bool close_stdin = false;
  bool close_stdout = false;
  bool close_stderr = false;
  std::array<char, PIPE_BUF> stdin_buffer = std::array<char, PIPE_BUF>();
  std::array<char, PIPE_BUF> stdout_buffer = std::array<char, PIPE_BUF>();
  std::array<char, PIPE_BUF> stderr_buffer = std::array<char, PIPE_BUF>();
};

process::process(const std::string &path, char *const argv)
    : pimpl(std::make_unique<impl>())
{
  static const int PIPE_READ = 0;
  static const int PIPE_WRITE = 1;

  errno = 0;

  std::array<int, 2> stdin_pipe = std::array<int, 2>();
  std::array<int, 2> stdout_pipe = std::array<int, 2>();
  std::array<int, 2> stderr_pipe = std::array<int, 2>();

  if (pipe(stdin_pipe.data()) == -1) { goto end; }
  pimpl->stdin = stdin_pipe[PIPE_WRITE];
  pimpl->close_stdin = true;

  if (pipe(stdout_pipe.data()) == -1) { goto end; }
  pimpl->stdout = stdout_pipe[PIPE_READ];
  pimpl->close_stdout = true;

  if (pipe(stderr_pipe.data()) == -1) { goto end; }
  pimpl->stderr = stderr_pipe[PIPE_READ];
  pimpl->close_stderr = true;

  _pid = fork();
  if (_pid == 0) {
    // In child process
    // Since we're in a child process we can exit on error
    // why _exit? See:
    // https://stackoverflow.com/questions/5422831/what-is-the-difference-between-using-exit-exit-in-a-conventional-linux-fo?noredirect=1&lq=1

    // redirect stdin, stdout and stderr

    if (dup2(stdin_pipe[PIPE_READ], STDIN_FILENO) == -1) {
      // _exit ensures open file descriptors (pipes) are closed
      _exit(errno);
    }

    if (dup2(stdout_pipe[PIPE_WRITE], STDOUT_FILENO) == -1) { _exit(errno); }

    if (dup2(stderr_pipe[PIPE_WRITE], STDERR_FILENO) == -1) { _exit(errno); }

    // We have no use anymore for the parent and copied pipe endpoints
    close(stdin_pipe[PIPE_READ]);
    close(stdin_pipe[PIPE_WRITE]);
    close(stdout_pipe[PIPE_READ]);
    close(stdout_pipe[PIPE_WRITE]);
    close(stderr_pipe[PIPE_READ]);
    close(stderr_pipe[PIPE_WRITE]);

    // Replace forked child with process we want to run
    execvp(path.data(), &argv);

    _exit(errno);
  }

end:
  // Close child endpoints of pipes (not needed by parent)
  if (pimpl->close_stdin) { close(pimpl->stdin); }
  if (pimpl->close_stdout) { close(pimpl->stdout); }
  if (pimpl->close_stderr) { close(pimpl->stderr); }

  if (errno != 0) {
    std::string error = std::strerror(errno);
    throw std::runtime_error(error);
  }
}

process::process(process &&other) noexcept { pimpl = std::move(other.pimpl); }

process &process::operator=(process &&other) noexcept
{
  if (this != &other) { pimpl = std::move(other.pimpl); }

  return *this;
}

process::~process()
{
  // Close parent endpoints of pipes if not in moved-from state
  if (pimpl) {
    if (pimpl->close_stdin) { close(pimpl->stdin); }
    if (pimpl->close_stdout) { close(pimpl->stdout); }
    if (pimpl->close_stderr) { close(pimpl->stderr); }
  }
}

#endif
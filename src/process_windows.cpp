#if defined(_WIN32)

#include "process.hpp"

#include <cassert>
#include <sstream>
#include <windows.h>

struct process::impl {
  bool close_stdin = false;
  bool close_stdout = false;
  bool close_stderr = false;
  // Suffix _ to avoid name clash errors
  HANDLE stdin_ = NULL;
  HANDLE stdout_ = NULL;
  HANDLE stderr_ = NULL;
};

process::process(const std::vector<std::wstring> &argv) : pimpl(new impl())
{
  assert(argv.size() > 0);
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
  static const int PIPE_READ = 0;
  static const int PIPE_WRITE = 1;

  HANDLE stdin_pipe[2];
  HANDLE stdout_pipe[2];
  HANDLE stderr_pipe[2];

  // Ensure pipes are inherited by child process. We disable inheriting of
  // specific pipes the child doesn't need (such as read end of stdout) later.
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attributes.bInheritHandle = TRUE;
  security_attributes.lpSecurityDescriptor = NULL;

  // Predeclare from here so we can use goto
  std::wstringstream command_line_builder;
  std::wstring command_line;
  wchar_t *non_const;

  PROCESS_INFORMATION process_info;
  STARTUPINFOW startup_info;

  BOOL result;

  if (!CreatePipe(&stdin_pipe[PIPE_READ], &stdin_pipe[PIPE_WRITE],
                  &security_attributes, 0)) {
    goto end;
  }
  pimpl->stdin_ = stdin_pipe[PIPE_WRITE];
  pimpl->close_stdin = true;

  // Ensure the write handle to the pipe for stdin is not inherited.
  if (!SetHandleInformation(stdin_pipe[PIPE_WRITE], HANDLE_FLAG_INHERIT, 0)) {
    goto end;
  }

  if (!CreatePipe(&stdout_pipe[PIPE_READ], &stdout_pipe[PIPE_WRITE],
                  &security_attributes, 0)) {
    goto end;
  }
  pimpl->stdout_ = stdout_pipe[PIPE_READ];
  pimpl->close_stdout = true;

  // Ensure the read handle to the pipe for stdout is not inherited.
  if (!SetHandleInformation(stdout_pipe[PIPE_READ], HANDLE_FLAG_INHERIT, 0)) {
    goto end;
  }

  if (!CreatePipe(&stderr_pipe[PIPE_READ], &stderr_pipe[PIPE_WRITE],
                  &security_attributes, 0)) {
    goto end;
  }
  pimpl->stderr_ = stderr_pipe[PIPE_READ];
  pimpl->close_stderr = true;

  // Ensure the read handle to the pipe for stderr is not inherited.
  if (!SetHandleInformation(stderr_pipe[PIPE_READ], HANDLE_FLAG_INHERIT, 0)) {
    goto end;
  }

  ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));

  ZeroMemory(&startup_info, sizeof(STARTUPINFO));
  startup_info.cb = sizeof(STARTUPINFO);
  startup_info.hStdInput = stdin_pipe[PIPE_READ];
  startup_info.hStdOutput = stdout_pipe[PIPE_WRITE];
  startup_info.hStdError = stderr_pipe[PIPE_WRITE];
  startup_info.dwFlags |= STARTF_USESTDHANDLES;

  // Join argv to whitespace delimited string (as required by CreateProcess)
  for (int i = 0; i < argv.size(); i++) {
    command_line_builder << argv[i];
    if (i != argv.size() - 1) { command_line_builder << u" "; }
  }
  command_line = command_line_builder.str();
  non_const = const_cast<wchar_t *>(command_line.c_str());

  result = CreateProcessW(NULL, non_const, NULL, NULL, TRUE, 0, NULL, NULL,
                          &startup_info, &process_info);

end:
  return;
}

#endif
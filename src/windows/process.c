#include "process.h"
#include "process_impl.h"
#include "util.h"

#include <assert.h>
#include <malloc.h>
#include <windows.h>

struct process *process_alloc(void) { return malloc(sizeof(struct process)); }

PROCESS_LIB_ERROR process_init(struct process *process)
{
  assert(process);

  ZeroMemory(&process->info, sizeof(PROCESS_INFORMATION));
  process->info.hThread = NULL;
  process->info.hProcess = NULL;
  process->info.dwProcessId = 0;

  process->stdin = NULL;
  process->stdout = NULL;
  process->stderr = NULL;
  process->child_stdin = NULL;
  process->child_stdout = NULL;
  process->child_stderr = NULL;

  // Continue allocating pipes until error occurs (PROCESS_SUCCESS = 0)
  PROCESS_LIB_ERROR error = PROCESS_LIB_SUCCESS;

  error = pipe_init(&process->child_stdin, &process->stdin);
  if (error) { return error; }
  error = pipe_disable_inherit(process->stdin);
  if (error) { return error; }

  error = pipe_init(&process->stdout, &process->child_stdout);
  if (error) { return error; }
  error = pipe_disable_inherit(process->stdout);
  if (error) { return error; }

  error = pipe_init(&process->stderr, &process->child_stderr);
  if (error) { return error; }
  error = pipe_disable_inherit(process->stderr);
  if (error) { return error; }

  return PROCESS_LIB_SUCCESS;
}

// Create each process in a new process group so we can send separate CTRL-BREAK
// signals to each of them
static const DWORD CREATION_FLAGS = CREATE_NEW_PROCESS_GROUP;

PROCESS_LIB_ERROR process_start(struct process *process, int argc, char *argv[])
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc]);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  // Make sure process_start is only called once for each process_init call
  assert(!process->info.hProcess);

  // Make sure process was initialized completely
  assert(process->stdin);
  assert(process->stdout);
  assert(process->stderr);
  assert(process->child_stdin);
  assert(process->child_stdout);
  assert(process->child_stderr);

  STARTUPINFOW startup_info;
  ZeroMemory(&startup_info, sizeof(STARTUPINFOW));
  startup_info.cb = sizeof(STARTUPINFOW);
  startup_info.dwFlags |= STARTF_USESTDHANDLES;

  // Assign child pipe endpoints to child process stdin/stdout/stderr
  startup_info.hStdInput = process->child_stdin;
  startup_info.hStdOutput = process->child_stdout;
  startup_info.hStdError = process->child_stderr;

  // Join argv to whitespace delimited string as required by CreateProcess
  char *command_line_string = string_join(argv, argc);
  // Convert utf-8 to utf-16 as required by CreateProcessW
  wchar_t *command_line_wstring = string_to_wstring(command_line_string);

  SetLastError(0);

  CreateProcessW(NULL, command_line_wstring, NULL, NULL, TRUE, CREATION_FLAGS,
                 NULL, NULL, &startup_info, &process->info);

  free(command_line_string);
  free(command_line_wstring);

  PROCESS_LIB_ERROR error = system_error_to_process_error(GetLastError());

  CloseHandle(process->info.hThread);

  CloseHandle(process->child_stdin);
  CloseHandle(process->child_stdout);
  CloseHandle(process->child_stderr);

  process->child_stdin = NULL;
  process->child_stdout = NULL;
  process->child_stderr = NULL;

  // CreateProcessW error has priority over CloseHandle errors
  error = error ? error : system_error_to_process_error(GetLastError());

  return error;
}

PROCESS_LIB_ERROR process_write(struct process *process, const void *buffer,
                                uint32_t to_write, uint32_t *actual)
{
  assert(process);
  assert(process->stdin);
  assert(buffer);
  assert(actual);

  return pipe_write(process->stdin, buffer, to_write, actual);
}

PROCESS_LIB_ERROR process_read(struct process *process, void *buffer,
                               uint32_t to_read, uint32_t *actual)
{
  assert(process);
  assert(process->stdout);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stdout, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_read_stderr(struct process *process, void *buffer,
                                      uint32_t to_read, uint32_t *actual)
{
  assert(process);
  assert(process->stderr);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stderr, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_wait(struct process *process, uint32_t milliseconds)
{
  assert(process);
  assert(process->info.hProcess);

  SetLastError(0);

  DWORD wait_result = WaitForSingleObject(process->info.hProcess, milliseconds);

  switch (wait_result) {
  case WAIT_TIMEOUT:
    return PROCESS_LIB_WAIT_TIMEOUT;
  case WAIT_FAILED:
    return system_error_to_process_error(GetLastError());
  }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_terminate(struct process *process,
                                    uint32_t milliseconds)
{
  assert(process);
  assert(process->info.dwProcessId);

  SetLastError(0);

  // process group of process started with CREATE_NEW_PROCESS_GROUP is equal to
  // the process id
  if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process->info.dwProcessId)) {
    return system_error_to_process_error(GetLastError());
  }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_kill(struct process *process, uint32_t milliseconds)
{
  assert(process);
  assert(process->info.hProcess);

  SetLastError(0);

  if (!TerminateProcess(process->info.hProcess, 1)) {
    return system_error_to_process_error(GetLastError());
  }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_exit_status(struct process *process,
                                      int32_t *exit_status)
{
  assert(process);
  assert(process->info.hProcess);

  errno = 0;

  DWORD unsigned_exit_status = 0;
  if (!GetExitCodeProcess(process->info.hProcess, &unsigned_exit_status)) {
    return system_error_to_process_error(GetLastError());
  }

  if (unsigned_exit_status == STILL_ACTIVE) {
    return PROCESS_LIB_STILL_RUNNING;
  }

  *exit_status = (int32_t) unsigned_exit_status;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_free(struct process *process)
{
  assert(process);

  SetLastError(0);

  // NULL checks so free works even if process was only partially initialized
  // (can happen if error occurs during initialization or if start is not
  // called)
  if (process->info.hProcess) { CloseHandle(process->info.hProcess); }

  if (process->stdin) { CloseHandle(process->stdin); }
  if (process->stdout) { CloseHandle(process->stdout); }
  if (process->stderr) { CloseHandle(process->stderr); }

  if (process->child_stdin) { CloseHandle(process->child_stdin); }
  if (process->child_stdout) { CloseHandle(process->child_stdout); }
  if (process->child_stderr) { CloseHandle(process->child_stderr); }

  PROCESS_LIB_ERROR error = system_error_to_process_error(GetLastError());

  return error;
}

int64_t process_system_error(void) { return GetLastError(); }

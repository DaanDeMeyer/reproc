#include "process.h"
#include "util.h"

#include <assert.h>
#include <windows.h>

typedef struct process {
  HANDLE stdin;
  HANDLE stdout;
  HANDLE stderr;
  PROCESS_INFORMATION info;
} process;

// Create each process in a new process group so we can send separate CTRL-BREAK
// signals to each of them 
static const DWORD CREATION_FLAGS = CREATE_NEW_PROCESS_GROUP;

PROCESS_ERROR process_init(process *process, int argc, char *argv[])
{
  assert(process != NULL);
  assert(argc > 0);
  assert(argv != NULL);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i] != NULL);
  }

  process->stdin = NULL;
  process->stdout = NULL;
  process->stderr = NULL;
  HANDLE child_stdin = NULL;
  HANDLE child_stdout = NULL;
  HANDLE child_stderr = NULL;

  ZeroMemory(&process->info, sizeof(PROCESS_INFORMATION));
  process->info.hThread = NULL;
  process->info.hProcess = NULL;

  STARTUPINFOW startup_info;
  ZeroMemory(&startup_info, sizeof(STARTUPINFOW));
  startup_info.cb = sizeof(STARTUPINFOW);
  startup_info.dwFlags |= STARTF_USESTDHANDLES;

  char *command_line_string = NULL;
  wchar_t *command_line_wstring = NULL;

  // Reset last error so we know for that if an error occurred it occurred in
  // this function 
  SetLastError(0);

  if (!pipe_init(&child_stdin, &process->stdin, &process->stdin) ||
      !pipe_init(&process->stdout, &child_stdout, &process->stdout) ||
      !pipe_init(&process->stderr, &child_stderr, &process->stderr)) {
    goto end;
  }

  // Assign child pipe endpoints to child process stdin/stdout/stderr
  startup_info.hStdInput = child_stdin;
  startup_info.hStdOutput = child_stdout;
  startup_info.hStdError = child_stderr;

  // Join argv to whitespace delimited string as required by CreateProcess
  command_line_string = string_join(argv, argc);
  // Convert utf-8 to utf-16 as required by CreateProcessW
  command_line_wstring = string_to_wstring(command_line_string);

  if (!CreateProcessW(NULL, command_line_wstring, NULL, NULL, TRUE,
                      CREATION_FLAGS, NULL, NULL, &startup_info,
                      &process->info)) {
    goto end;
  }

end:
  if (command_line_string) { free(command_line_string); }
  if (command_line_wstring) { free(command_line_wstring); }

  // We don't need the handle to the child process main thread
  if (process->info.hThread) { CloseHandle(process->info.hThread); }

  PROCESS_ERROR error = system_error_to_process_error(GetLastError());

  // Free all allocated resources if an error occurred while setting up the
  // child process
  if (error != PROCESS_SUCCESS) {
    if (child_stdin) { CloseHandle(child_stdin); }
    if (child_stdout) { CloseHandle(child_stdout); }
    if (child_stderr) { CloseHandle(child_stderr); }

    process_free(process);
  }

  return error;
}

PROCESS_ERROR process_free(process *process)
{
  // If process was succesfully created child pipe endpoints will be closed when
  // the child process exits
  if (process->stdin) { CloseHandle(process->stdin); }
  if (process->stdout) { CloseHandle(process->stdout); }
  if (process->stderr) { CloseHandle(process->stderr); }

  if (process->info.hProcess) { CloseHandle(process->info.hProcess); }

  return PROCESS_SUCCESS;
}

PROCESS_ERROR process_wait(process *process, uint32_t milliseconds)
{
  DWORD wait_result = WaitForSingleObject(process->info.hProcess, milliseconds);

  switch (wait_result) {
  case WAIT_TIMEOUT:
    return PROCESS_WAIT_TIMEOUT;
  case WAIT_FAILED:
    return system_error_to_process_error(GetLastError());
  }

  return PROCESS_SUCCESS;
}

PROCESS_ERROR process_terminate(process *process, uint32_t milliseconds)
{
  // Process group of process started with CREATE_NEW_PROCESS_GROUP is equal to
  // the process id
  if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process->info.dwProcessId)) {
    return system_error_to_process_error(GetLastError());
  }

  return process_wait(process, milliseconds);
}

PROCESS_ERROR process_kill(process *process, uint32_t milliseconds)
{
  if (!TerminateProcess(process->info.hProcess, 0)) {
    return system_error_to_process_error(GetLastError());
  }

  return process_wait(process, milliseconds);
}

PROCESS_ERROR process_write_stdin(process *process, const void *buffer,
                                  uint32_t to_write, uint32_t *actual)
{
  return pipe_write(process->stdin, buffer, to_write, actual);
}

PROCESS_ERROR process_read_stdout(process *process, void *buffer,
                                  uint32_t to_read, uint32_t *actual)
{
  return pipe_read(process->stdout, buffer, to_read, actual);
}

PROCESS_ERROR process_read_stderr(process *process, void *buffer,
                                  uint32_t to_read, uint32_t *actual)
{
  return pipe_read(process->stderr, buffer, to_read, actual);
}

int64_t process_system_error(void) { return GetLastError(); }

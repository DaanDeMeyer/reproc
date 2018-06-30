#include "process.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <windows.h>

struct process {
  PROCESS_INFORMATION info;
  HANDLE stdin;
  HANDLE stdout;
  HANDLE stderr;
  HANDLE child_stdin;
  HANDLE child_stdout;
  HANDLE child_stderr;
};

unsigned int process_size() { return sizeof(struct process); }

PROCESS_LIB_ERROR process_init(struct process *process)
{
  assert(process);

  process->info.hThread = NULL;
  process->info.hProcess = NULL;
  // process id 0 is reserved by the system so we can use it as a null value
  process->info.dwProcessId = 0;

  process->stdin = NULL;
  process->stdout = NULL;
  process->stderr = NULL;
  process->child_stdin = NULL;
  process->child_stdout = NULL;
  process->child_stderr = NULL;

  return PROCESS_LIB_SUCCESS;
}

// Create each process in a new process group so we don't send CTRL-BREAK
// signals to more than one child process in process_terminate.
static const DWORD CREATION_FLAGS = CREATE_NEW_PROCESS_GROUP;

PROCESS_LIB_ERROR process_start(struct process *process, const char *argv[],
                                int argc, const char *working_directory)
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  // Make sure process_start is only called once for each process_init call
  assert(!process->info.hProcess);
  assert(!process->info.dwProcessId);

  PROCESS_LIB_ERROR error;

  // pipe_init makes the child process inherit both the read and write handle of
  // each pipe but the child process only needs one handle (read for stdin,
  // write for stdout/stderr) for each pipe so we disable inheritance of the
  // handle that is not needed with pipe_disable_inherit
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

  // Join argv to whitespace delimited string as required by CreateProcess
  char *command_line_string = NULL;
  error = string_join(argv, argc, &command_line_string);
  if (error) { return error; }

  // Convert utf-8 to utf-16 as required by CreateProcessW
  wchar_t *command_line_wstring = NULL;
  error = string_to_wstring(command_line_string, &command_line_wstring);
  free(command_line_string); // Not needed anymore
  if (error) { return error; }

  // Do the same for the working directory string if one was provided
  wchar_t *working_directory_wstring = NULL;
  error = working_directory
              ? string_to_wstring(working_directory, &working_directory_wstring)
              : PROCESS_LIB_SUCCESS;
  if (error) {
    free(command_line_wstring);
    return error;
  }

  // Following code is based on:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx

  STARTUPINFOW startup_info;
  ZeroMemory(&startup_info, sizeof(STARTUPINFOW));
  startup_info.cb = sizeof(STARTUPINFOW);
  startup_info.dwFlags = STARTF_USESTDHANDLES;

  // Assign child pipe endpoints to child process stdin/stdout/stderr
  startup_info.hStdInput = process->child_stdin;
  startup_info.hStdOutput = process->child_stdout;
  startup_info.hStdError = process->child_stderr;

  // Child processes inherit error mode of their parents. To avoid child
  // processes creating error dialogs we set our error mode to not create error
  // dialogs temporarily.
  DWORD previous_error_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX);

  SetLastError(0);
  BOOL result = CreateProcessW(NULL, command_line_wstring, NULL, NULL, TRUE,
                               CREATION_FLAGS, NULL, working_directory_wstring,
                               &startup_info, &process->info);

  SetErrorMode(previous_error_mode);

  // Free allocated memory before returning possible error
  free(command_line_wstring);
  free(working_directory_wstring);

  if (!result) { return PROCESS_LIB_UNKNOWN_ERROR; }

  // We don't need the handle to the primary thread of the child process
  error = handle_close(&process->info.hThread);
  if (error) { return error; }

  // The child process handles are copied to the child process. We close them in
  // the parent so their resources are freed once the child process exits
  error = handle_close(&process->child_stdin);
  if (error) { return error; }
  error = handle_close(&process->child_stdout);
  if (error) { return error; }
  error = handle_close(&process->child_stderr);
  if (error) { return error; }

  return error;
}

PROCESS_LIB_ERROR process_write(struct process *process, const void *buffer,
                                unsigned int to_write, unsigned int *actual)
{
  assert(process);
  assert(process->stdin);
  assert(buffer);
  assert(actual);

  return pipe_write(process->stdin, buffer, to_write, actual);
}

PROCESS_LIB_ERROR process_close_stdin(struct process *process)
{
  assert(process);
  assert(process->stdin);

  return handle_close(&process->stdin);
}

PROCESS_LIB_ERROR process_read(struct process *process, void *buffer,
                               unsigned int to_read, unsigned int *actual)
{
  assert(process);
  assert(process->stdout);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stdout, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_read_stderr(struct process *process, void *buffer,
                                      unsigned int to_read,
                                      unsigned int *actual)
{
  assert(process);
  assert(process->stderr);
  assert(buffer);
  assert(actual);

  return pipe_read(process->stderr, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_wait(struct process *process,
                               unsigned int milliseconds)
{
  assert(process);
  assert(process->info.hProcess);

  SetLastError(0);
  DWORD wait_result = WaitForSingleObject(process->info.hProcess, milliseconds);
  if (wait_result == WAIT_TIMEOUT) { return PROCESS_LIB_WAIT_TIMEOUT; }
  if (wait_result == WAIT_FAILED) { return PROCESS_LIB_UNKNOWN_ERROR; }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_terminate(struct process *process,
                                    unsigned int milliseconds)
{
  assert(process);
  assert(process->info.dwProcessId);

  // Check if child process has already exited before sending signal
  PROCESS_LIB_ERROR error = process_wait(process, 0);

  // Return if wait succeeds (which means process has already exited) or if
  // an error other than a wait timeout occurs during waiting
  if (error != PROCESS_LIB_WAIT_TIMEOUT) { return error; }

  // GenerateConsoleCtrlEvent can only be passed a process group id. This is why
  // we start each child process in its own process group (which has the same id
  // as the child process id) so we can call GenerateConsoleCtrlEvent on single
  // child processes
  SetLastError(0);
  if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, process->info.dwProcessId)) {
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_kill(struct process *process,
                               unsigned int milliseconds)
{
  assert(process);
  assert(process->info.hProcess);

  // Check if child process has already exited before sending signal
  PROCESS_LIB_ERROR error = process_wait(process, 0);

  // Return if wait succeeds (which means process has already exited) or if
  // an error other than a wait timeout occurs during waiting
  if (error != PROCESS_LIB_WAIT_TIMEOUT) { return error; }

  SetLastError(0);
  if (!TerminateProcess(process->info.hProcess, 1)) {
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  return process_wait(process, milliseconds);
}

PROCESS_LIB_ERROR process_exit_status(struct process *process, int *exit_status)
{
  assert(process);
  assert(process->info.hProcess);

  DWORD unsigned_exit_status = 0;
  SetLastError(0);
  if (!GetExitCodeProcess(process->info.hProcess, &unsigned_exit_status)) {
    return PROCESS_LIB_UNKNOWN_ERROR;
  }

  if (unsigned_exit_status == STILL_ACTIVE) {
    return PROCESS_LIB_STILL_RUNNING;
  }

  *exit_status = unsigned_exit_status;

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_free(struct process *process)
{
  assert(process);

  // All resources are closed regardless of errors but only the first error
  // is returned
  PROCESS_LIB_ERROR result = PROCESS_LIB_SUCCESS;
  PROCESS_LIB_ERROR error;

  error = handle_close(&process->info.hThread);
  if (!result) { result = error; }
  error = handle_close(&process->info.hProcess);
  if (!result) { result = error; }

  error = handle_close(&process->stdin);
  if (!result) { result = error; }
  error = handle_close(&process->stdout);
  if (!result) { result = error; }
  error = handle_close(&process->stderr);
  if (!result) { result = error; }

  error = handle_close(&process->child_stdin);
  if (!result) { result = error; }
  error = handle_close(&process->child_stdout);
  if (!result) { result = error; }
  error = handle_close(&process->child_stderr);
  if (!result) { result = error; }

  return result;
}

unsigned int process_system_error(void) { return GetLastError(); }

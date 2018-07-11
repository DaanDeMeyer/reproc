#include "process.h"

#include "handle.h"
#include "pipe.h"
#include "string_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <windows.h>

const unsigned int PROCESS_LIB_INFINITE = 0xFFFFFFFF;

struct process {
  PROCESS_INFORMATION info;
  HANDLE parent_stdin;
  HANDLE parent_stdout;
  HANDLE parent_stderr;
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
  process->info.dwThreadId = 0;

  process->parent_stdin = NULL;
  process->parent_stdout = NULL;
  process->parent_stderr = NULL;
  process->child_stdin = NULL;
  process->child_stdout = NULL;
  process->child_stderr = NULL;

  PROCESS_LIB_ERROR error;

  // While we already make sure the child process only inherits the child pipe
  // handles using STARTUPINFOEXW (see further in this function) we still
  // disable inheritance of the parent pipe handles to lower the chance of other
  // CreateProcess calls (outside of this library) unintentionally inheriting
  // these handles.
  error = pipe_init(&process->child_stdin, &process->parent_stdin);
  if (error) { return error; }
  error = pipe_disable_inherit(process->parent_stdin);
  if (error) { return error; }

  error = pipe_init(&process->parent_stdout, &process->child_stdout);
  if (error) { return error; }
  error = pipe_disable_inherit(process->parent_stdout);
  if (error) { return error; }

  error = pipe_init(&process->parent_stderr, &process->child_stderr);
  if (error) { return error; }
  error = pipe_disable_inherit(process->parent_stderr);
  if (error) { return error; }

  return PROCESS_LIB_SUCCESS;
}

// Create each process in a new process group so we don't send CTRL-BREAK
// signals to more than one child process in process_terminate.
static const DWORD CREATION_FLAGS = CREATE_NEW_PROCESS_GROUP |
                                    EXTENDED_STARTUPINFO_PRESENT;

PROCESS_LIB_ERROR process_start(struct process *process, int argc,
                                const char *argv[],
                                const char *working_directory)
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  // Make sure process_start is only called once for each process_init call
  assert(process->info.hProcess != NULL);

  PROCESS_LIB_ERROR error;

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

  // To ensure no handles other than those necessary are inherited we use the
  // approach detailed in https://stackoverflow.com/a/2345126
  HANDLE to_inherit[3] = { process->child_stdin, process->child_stdout,
                           process->child_stderr };
  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list;
  error = handle_inherit_list_create(to_inherit, 3, &attribute_list);
  if (error) {
    free(command_line_wstring);
    free(working_directory_wstring);
    return error;
  }

  STARTUPINFOEXW startup_info =
      { .StartupInfo = { .cb = sizeof(startup_info),
                         .dwFlags = STARTF_USESTDHANDLES,
                         .hStdInput = process->child_stdin,
                         .hStdOutput = process->child_stdout,
                         .hStdError = process->child_stderr },
        .lpAttributeList = attribute_list };

  // Child processes inherit error mode of their parents. To avoid child
  // processes creating error dialogs we set our error mode to not create error
  // dialogs temporarily.
  DWORD previous_error_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX);

  SetLastError(0);
  BOOL result = CreateProcessW(NULL, command_line_wstring, NULL, NULL, TRUE,
                               CREATION_FLAGS, NULL, working_directory_wstring,
                               &startup_info.StartupInfo, &process->info);

  SetErrorMode(previous_error_mode);

  // Free allocated memory before returning possible error
  free(command_line_wstring);
  free(working_directory_wstring);
  free(attribute_list);

  if (!result) {
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND: return PROCESS_LIB_FILE_NOT_FOUND;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  // We don't need the handle to the primary thread of the child process
  handle_close(&process->info.hThread);

  // The child process pipe endpoint handles are copied to the child process. We
  // don't need them anymore in the parent process so we close them.
  handle_close(&process->child_stdin);
  handle_close(&process->child_stdout);
  handle_close(&process->child_stderr);

  return error;
}

PROCESS_LIB_ERROR process_write(struct process *process, const void *buffer,
                                unsigned int to_write, unsigned int *actual)
{
  assert(process);
  assert(process->parent_stdin);
  assert(buffer);
  assert(actual);

  return pipe_write(process->parent_stdin, buffer, to_write, actual);
}

PROCESS_LIB_ERROR process_close_stdin(struct process *process)
{
  assert(process);
  assert(process->parent_stdin);

  handle_close(&process->parent_stdin);

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR process_read(struct process *process, void *buffer,
                               unsigned int to_read, unsigned int *actual)
{
  assert(process);
  assert(process->parent_stdout);
  assert(buffer);
  assert(actual);

  return pipe_read(process->parent_stdout, buffer, to_read, actual);
}

PROCESS_LIB_ERROR process_read_stderr(struct process *process, void *buffer,
                                      unsigned int to_read,
                                      unsigned int *actual)
{
  assert(process);
  assert(process->parent_stderr);
  assert(buffer);
  assert(actual);

  return pipe_read(process->parent_stderr, buffer, to_read, actual);
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

PROCESS_LIB_ERROR process_destroy(struct process *process)
{
  assert(process);

  handle_close(&process->info.hThread);
  handle_close(&process->info.hProcess);

  handle_close(&process->parent_stdin);
  handle_close(&process->parent_stdout);
  handle_close(&process->parent_stderr);

  handle_close(&process->child_stdin);
  handle_close(&process->child_stdout);
  handle_close(&process->child_stderr);

  return PROCESS_LIB_SUCCESS;
}

unsigned int process_system_error(void) { return GetLastError(); }

PROCESS_LIB_ERROR process_system_error_string(char **error_string)
{
  PROCESS_LIB_ERROR error;

  wchar_t *message_wstring = NULL;
  int result = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                  FORMAT_MESSAGE_FROM_SYSTEM |
                                  FORMAT_MESSAGE_IGNORE_INSERTS,
                              NULL, GetLastError(),
                              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              (LPWSTR) &message_wstring, 0, NULL);

  if (!result) { return PROCESS_LIB_UNKNOWN_ERROR; }

  error = wstring_to_string(message_wstring, error_string);
  free(message_wstring);
  if (error) { return error; }

  return PROCESS_LIB_SUCCESS;
}

void process_system_error_string_free(char *error_string)
{
  free(error_string);
}

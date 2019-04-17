#include "process.h"
#include "handle.h"

#include <assert.h>

#if defined(ATTRIBUTE_LIST_FOUND)
#include <stdlib.h>

REPROC_ERROR
static handle_inherit_list_create(HANDLE *handles,
                                  int amount,
                                  LPPROC_THREAD_ATTRIBUTE_LIST *result)
{
  assert(handles);
  assert(amount >= 0);
  assert(result);

  // Get the required size for `attribute_list`.
  SIZE_T attribute_list_size = 0;
  if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return REPROC_UNKNOWN_ERROR;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = malloc(attribute_list_size);
  if (!attribute_list) {
    return REPROC_NOT_ENOUGH_MEMORY;
  }

  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0,
                                         &attribute_list_size)) {
    free(attribute_list);
    return REPROC_UNKNOWN_ERROR;
  }

  // Add the handles to be inherited to `attribute_list`.
  if (!UpdateProcThreadAttribute(attribute_list, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                 amount * sizeof(HANDLE), NULL, NULL)) {
    DeleteProcThreadAttributeList(attribute_list);
    return REPROC_UNKNOWN_ERROR;
  }

  *result = attribute_list;

  return REPROC_SUCCESS;
}
#endif

REPROC_ERROR process_create(wchar_t *command_line,
                            struct process_options *options,
                            DWORD *pid,
                            HANDLE *handle)
{
  assert(command_line);
  assert(options);
  assert(pid);
  assert(handle);

  // Create each child process in a new process group so we don't send
  // `CTRL-BREAK` signals to more than one child process in `process_terminate`.
  DWORD creation_flags = CREATE_NEW_PROCESS_GROUP;

#if defined(ATTRIBUTE_LIST_FOUND)
  REPROC_ERROR error = REPROC_SUCCESS;

  // To ensure no handles other than those necessary are inherited we use the
  // approach detailed in https://stackoverflow.com/a/2345126.
  HANDLE to_inherit[3];
  int i = 0; // to_inherit_size

  if (options->stdin_handle) {
    to_inherit[i++] = options->stdin_handle;
  }

  if (options->stdout_handle) {
    to_inherit[i++] = options->stdout_handle;
  }

  if (options->stderr_handle) {
    to_inherit[i++] = options->stderr_handle;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = NULL;
  error = handle_inherit_list_create(to_inherit, i, &attribute_list);
  if (error) {
    return error;
  }

  creation_flags |= EXTENDED_STARTUPINFO_PRESENT;

  STARTUPINFOEXW extended_startup_info = {
    .StartupInfo = { .cb = sizeof(extended_startup_info),
                     .dwFlags = STARTF_USESTDHANDLES,
                     .hStdInput = options->stdin_handle,
                     .hStdOutput = options->stdout_handle,
                     .hStdError = options->stderr_handle },
    .lpAttributeList = attribute_list
  };

  LPSTARTUPINFOW startup_info_address = &extended_startup_info.StartupInfo;
#else
  STARTUPINFOW startup_info = { .cb = sizeof(startup_info),
                                .dwFlags = STARTF_USESTDHANDLES,
                                .hStdInput = options->stdin_handle,
                                .hStdOutput = options->stdout_handle,
                                .hStdError = options->stderr_handle };

  LPSTARTUPINFOW startup_info_address = &startup_info;
#endif

  // Make sure the console window of the child process isn't visible. See
  // https://github.com/DaanDeMeyer/reproc/issues/6 and
  // https://github.com/DaanDeMeyer/reproc/pull/7 for more information.
  startup_info_address->dwFlags |= STARTF_USESHOWWINDOW;
  startup_info_address->wShowWindow = SW_HIDE;

  PROCESS_INFORMATION info;

  // Child processes inherit the error mode of their parents. To avoid child
  // processes creating error dialogs we set our error mode to not create error
  // dialogs temporarily which is inherited by the child process.
  DWORD previous_error_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX);

  BOOL result = CreateProcessW(NULL, command_line, NULL, NULL, TRUE,
                               creation_flags, NULL, options->working_directory,
                               startup_info_address, &info);

  SetErrorMode(previous_error_mode);

#if defined(ATTRIBUTE_LIST_FOUND)
  DeleteProcThreadAttributeList(attribute_list);
#endif

  // We don't need the handle to the primary thread of the child process.
  handle_close(&info.hThread);

  if (!result) {
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND:
      return REPROC_FILE_NOT_FOUND;
    default:
      return REPROC_UNKNOWN_ERROR;
    }
  }

  *pid = info.dwProcessId;
  *handle = info.hProcess;

  return REPROC_SUCCESS;
}

REPROC_ERROR
process_wait(HANDLE process, unsigned int timeout, unsigned int *exit_status)
{
  assert(process);
  assert(exit_status);

  DWORD wait_result = WaitForSingleObject(process, timeout);
  if (wait_result == WAIT_TIMEOUT) {
    return REPROC_WAIT_TIMEOUT;
  } else if (wait_result == WAIT_FAILED) {
    return REPROC_UNKNOWN_ERROR;
  }

  // `DWORD` is a typedef to `unsigned int` on Windows so the cast is safe.
  if (!GetExitCodeProcess(process, (LPDWORD) exit_status)) {
    return REPROC_UNKNOWN_ERROR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_terminate(unsigned long pid)
{
  // `GenerateConsoleCtrlEvent` can only be called on a process group. To call
  // `GenerateConsoleCtrlEvent` on a single child process it has to be put in
  // its own process group (which we did when starting the child process).
  if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)) {
    return REPROC_UNKNOWN_ERROR;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_kill(HANDLE process)
{
  assert(process);

  // We use 137 as the exit status because it is the same exit status as a
  // process that is stopped with the `SIGKILL` signal on POSIX systems.
  if (!TerminateProcess(process, 137)) {
    return REPROC_UNKNOWN_ERROR;
  }

  return REPROC_SUCCESS;
}

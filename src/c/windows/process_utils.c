#include "process_utils.h"

#include "handle.h"

#include <assert.h>

// Create each process in a new process group so we don't send CTRL-BREAK
// signals to more than one child process in process_terminate.
static const DWORD CREATION_FLAGS = CREATE_NEW_PROCESS_GROUP |
                                    EXTENDED_STARTUPINFO_PRESENT;

PROCESS_LIB_ERROR process_create(wchar_t *command_line,
                                 wchar_t *working_directory, HANDLE child_stdin,
                                 HANDLE child_stdout, HANDLE child_stderr,
                                 PROCESS_INFORMATION *info)
{
  assert(command_line);
  assert(child_stdin);
  assert(child_stdout);
  assert(child_stderr);
  assert(info);

  PROCESS_LIB_ERROR error;

  // To ensure no handles other than those necessary are inherited we use the
  // approach detailed in https://stackoverflow.com/a/2345126
  HANDLE to_inherit[3] = { child_stdin, child_stdout, child_stderr };
  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list;
  error = handle_inherit_list_create(to_inherit,
                                     sizeof(to_inherit) / sizeof(to_inherit[0]),
                                     &attribute_list);
  if (error) { return error; }

  STARTUPINFOEXW startup_info = { .StartupInfo = { .cb = sizeof(startup_info),
                                                   .dwFlags =
                                                       STARTF_USESTDHANDLES,
                                                   .hStdInput = child_stdin,
                                                   .hStdOutput = child_stdout,
                                                   .hStdError = child_stderr },
                                  .lpAttributeList = attribute_list };

  // Child processes inherit error mode of their parents. To avoid child
  // processes creating error dialogs we set our error mode to not create error
  // dialogs temporarily.
  DWORD previous_error_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX);

  SetLastError(0);
  BOOL result = CreateProcessW(NULL, command_line, NULL, NULL, TRUE,
                               CREATION_FLAGS, NULL, working_directory,
                               &startup_info.StartupInfo, info);

  SetErrorMode(previous_error_mode);

  DeleteProcThreadAttributeList(attribute_list);

  if (!result) {
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND: return PROCESS_LIB_FILE_NOT_FOUND;
    default: return PROCESS_LIB_UNKNOWN_ERROR;
    }
  }

  return PROCESS_LIB_SUCCESS;
}

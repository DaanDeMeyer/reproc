#include "process.h"
#include "handle.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

// Argument escaping implementation is based on the following blog post:
// https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/

static bool argument_should_escape(const char *argument)
{
  assert(argument);

  bool should_escape = false;

  for (size_t i = 0; i < strlen(argument); i++) {
    should_escape = should_escape || argument[i] == ' ' ||
                    argument[i] == '\t' || argument[i] == '\n' ||
                    argument[i] == '\v' || argument[i] == '\"';
  }

  return should_escape;
}

static size_t argument_escaped_size(const char *argument)
{
  assert(argument);

  if (!argument_should_escape(argument)) {
    return strlen(argument);
  }

  size_t size = 2; // double quotes

  for (size_t i = 0; i < strlen(argument); i++) {
    size_t num_backslashes = 0;

    while (i < strlen(argument) && argument[i] == '\\') {
      i++;
      num_backslashes++;
    }

    if (i == strlen(argument)) {
      size += num_backslashes * 2;
    } else if (argument[i] == '"') {
      size += num_backslashes * 2 + 1;
    } else {
      size += num_backslashes + 1;
    }
  }

  return size;
}

static size_t argument_escape(char *dest, const char *argument)
{
  assert(dest);
  assert(argument);

  if (!argument_should_escape(argument)) {
    memcpy(dest, argument, strlen(argument));
    return strlen(argument);
  }

  const char *begin = dest;

  *dest++ = '"';

  for (size_t i = 0; i < strlen(argument); i++) {
    size_t num_backslashes = 0;

    while (i < strlen(argument) && argument[i] == '\\') {
      i++;
      num_backslashes++;
    }

    if (i == strlen(argument)) {
      memset(dest, '\\', num_backslashes * 2);
      dest += num_backslashes * 2;
    } else if (argument[i] == '"') {
      memset(dest, '\\', num_backslashes * 2 + 1);
      dest += num_backslashes * 2 + 1;
      *dest++ = '"';
    } else {
      memset(dest, '\\', num_backslashes);
      dest += num_backslashes;
      *dest++ = argument[i];
    }
  }

  *dest++ = '"';

  return (size_t)(dest - begin);
}

char *argv_join(const char *const *argv)
{
  assert(argv);

  // Determine the size of the concatenated string first.
  size_t joined_size = 1; // Count the null terminator.
  for (int i = 0; argv[i] != NULL; i++) {
    joined_size += argument_escaped_size(argv[i]);

    if (argv[i + 1] != NULL) {
      joined_size++; // Count whitespace.
    }
  }

  char *joined = malloc(sizeof(char) * joined_size);
  if (joined == NULL) {
    return NULL;
  }

  char *current = joined;
  for (int i = 0; argv[i] != NULL; i++) {
    current += argument_escape(current, argv[i]);

    // We add a space after every part of the joined except for the last one.
    if (argv[i + 1] != NULL) {
      *current++ = ' ';
    }
  }

  *current = '\0';

  return joined;
}

size_t environment_join_size(const char *const *environment)
{
  size_t joined_size = 1; // Count the null terminator.
  for (int i = 0; environment[i] != NULL; i++) {
    joined_size += strlen(environment[i]) + 1; // Count the null terminator.
  }

  return joined_size;
}

char *environment_join(const char *const *environment)
{
  assert(environment);

  char *joined = malloc(sizeof(char) * environment_join_size(environment));
  if (joined == NULL) {
    return NULL;
  }

  char *current = joined;
  for (int i = 0; environment[i] != NULL; i++) {
    size_t to_copy = strlen(environment[i]) + 1; // Include null terminator.
    memcpy(current, environment[i], to_copy);
    current += to_copy;
  }

  *current = '\0';

  return joined;
}

wchar_t *string_to_wstring(const char *string, size_t size)
{
  assert(string);
  // Overflow check although we really don't expect this to ever happen. This
  // makes the following casts to `int` safe.
  assert(size <= INT_MAX);

  // Determine wstring size (`MultiByteToWideChar` returns the required size if
  // its last two arguments are `NULL` and 0).
  int rv = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, string,
                               (int) size, NULL, 0);
  if (rv == 0) {
    return NULL;
  }

  // `MultiByteToWideChar` does not return negative values so the cast to
  // `size_t` is safe.
  wchar_t *wstring = malloc(sizeof(wchar_t) * (size_t) rv);
  if (wstring == NULL) {
    return NULL;
  }

  // Now we pass our allocated string and its size as the last two arguments
  // instead of `NULL` and 0 which makes `MultiByteToWideChar` actually perform
  // the conversion.
  rv = MultiByteToWideChar(CP_UTF8, 0, string, (int) size, wstring, rv);
  if (rv == 0) {
    free(wstring);
    return NULL;
  }

  return wstring;
}

#include <stdlib.h>

static REPROC_ERROR handle_inherit_list_create(
    HANDLE *handles, size_t amount, LPPROC_THREAD_ATTRIBUTE_LIST *result)
{
  assert(handles);
  assert(result);

  // Get the required size for `attribute_list`.
  SIZE_T attribute_list_size = 0;
  if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return REPROC_ERROR_SYSTEM;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = malloc(attribute_list_size);
  if (!attribute_list) {
    return REPROC_ERROR_SYSTEM;
  }

  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0,
                                         &attribute_list_size)) {
    free(attribute_list);
    return REPROC_ERROR_SYSTEM;
  }

  // Add the handles to be inherited to `attribute_list`.
  if (!UpdateProcThreadAttribute(attribute_list, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                 amount * sizeof(HANDLE), NULL, NULL)) {
    DeleteProcThreadAttributeList(attribute_list);
    return REPROC_ERROR_SYSTEM;
  }

  *result = attribute_list;

  return REPROC_SUCCESS;
}

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
  // Create each child process with a Unicode environment as we accept any
  // UTF-16 encoded environment (including Unicode characters).
  DWORD creation_flags = CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT;

  REPROC_ERROR error = REPROC_SUCCESS;

  // Windows Vista added the `STARTUPINFOEXW` structure in which we can put a
  // list of handles that should be inherited. Only these handles are inherited
  // by the child process. Other code in an application that calls
  // `CreateProcess` without passing a `STARTUPINFOEXW` struct containing the
  // handles it should inherit can still unintentionally inherit handles meant
  // for a reproc child process. See https://stackoverflow.com/a/2345126 for
  // more information.

  HANDLE to_inherit[3];
  size_t i = 0; // to_inherit_size

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
                               creation_flags, options->environment,
                               options->working_directory, startup_info_address,
                               &info);

  SetErrorMode(previous_error_mode);

  DeleteProcThreadAttributeList(attribute_list);

  // We don't need the handle to the primary thread of the child process.
  handle_close(&info.hThread);

  if (!result) {
    return REPROC_ERROR_SYSTEM;
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
    return REPROC_ERROR_WAIT_TIMEOUT;
  } else if (wait_result == WAIT_FAILED) {
    return REPROC_ERROR_SYSTEM;
  }

  // `DWORD` is a typedef to `unsigned int` on Windows so the cast is safe.
  if (!GetExitCodeProcess(process, (LPDWORD) exit_status)) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_terminate(unsigned long pid)
{
  // `GenerateConsoleCtrlEvent` can only be called on a process group. To call
  // `GenerateConsoleCtrlEvent` on a single child process it has to be put in
  // its own process group (which we did when starting the child process).
  if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR process_kill(HANDLE process)
{
  assert(process);

  // We use 137 as the exit status because it is the same exit status as a
  // process that is stopped with the `SIGKILL` signal on POSIX systems.
  if (!TerminateProcess(process, 137)) {
    return REPROC_ERROR_SYSTEM;
  }

  return REPROC_SUCCESS;
}

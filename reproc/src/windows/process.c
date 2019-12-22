#include <process.h>

#include <macro.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>

static const DWORD CREATION_FLAGS =
    // Create each child process in a new process group so we don't send
    // `CTRL-BREAK` signals to more than one child process in
    // `process_terminate`.
    CREATE_NEW_PROCESS_GROUP |
    // Create each child process with a Unicode environment as we accept any
    // UTF-16 encoded environment (including Unicode characters). Create each
    CREATE_UNICODE_ENVIRONMENT |
    // Create each child with an extended STARTUPINFOEXW structure so we can
    // specify which handles should be inherited.
    EXTENDED_STARTUPINFO_PRESENT |
    // After starting the process using `CreateProcessW`, `process_create` can
    // still fail to assign the process to the job object. We only want the
    // process to start executing if `process_create` succeeds, so we create a
    // suspended process to make sure no code in the child process is executed
    // until we successfully assigned the process to the job object.
    CREATE_SUSPENDED;

static SECURITY_ATTRIBUTES HANDLE_DO_NOT_INHERIT = {
  .nLength = sizeof(SECURITY_ATTRIBUTES),
  .bInheritHandle = false,
  .lpSecurityDescriptor = NULL
};

static const DWORD SIGKILL = 137;

// To wait for any of multiple processes to exit on a single thread, we use a
// job object combined with a completion port. After registering the completion
// port with the job object, we can use `GetQueuedCompletionStatus` on the
// completion port to receive a notification when a process assigned to the job
// object exits. Because a process cannot be removed from a job object unless it
// exits, we cannot create job objects as needed in `process_wait`. Instead, we
// add each process to a single job object when it is created. The job object
// and completion port are created when the first process is created in
// `process_create` and are destroyed when the last process is destroyed in
// `process_destroy`. We use thread-local job objects and completion ports
// because if we used a global completion port, threads waiting simultaneously
// on different processes could receive the notifications for the other thread's
// processes, possibly leading to infinite waits even though the processes might
// have exited since another thread 'stole' the notification. This approach has
// the unfortunate side effect that a process cannot be waited on outside of the
// thread where it was created.
static THREAD_LOCAL unsigned long job_reference_count = 0;
static THREAD_LOCAL HANDLE job_object = NULL;
static THREAD_LOCAL HANDLE job_completion_port = NULL;

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

  size_t argument_size = strlen(argument);

  if (!argument_should_escape(argument)) {
    return argument_size;
  }

  size_t size = 2; // double quotes

  for (size_t i = 0; i < argument_size; i++) {
    size_t num_backslashes = 0;

    while (i < argument_size && argument[i] == '\\') {
      i++;
      num_backslashes++;
    }

    if (i == argument_size) {
      size += num_backslashes * 2;
    } else if (argument[i] == '"') {
      size += num_backslashes * 2 + 2;
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

  size_t argument_size = strlen(argument);

  if (!argument_should_escape(argument)) {
    memcpy(dest, argument, argument_size);
    return argument_size;
  }

  const char *begin = dest;

  *dest++ = '"';

  for (size_t i = 0; i < argument_size; i++) {
    size_t num_backslashes = 0;

    while (i < argument_size && argument[i] == '\\') {
      i++;
      num_backslashes++;
    }

    if (i == argument_size) {
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

static char *argv_join(const char *const *argv)
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

  char *joined = calloc(joined_size, sizeof(char));
  if (joined == NULL) {
    return NULL;
  }

  char *current = joined;
  for (int i = 0; argv[i] != NULL; i++) {
    current += argument_escape(current, argv[i]);

    // We add a space after each argument in the joined arguments string except
    // for the final argument.
    if (argv[i + 1] != NULL) {
      *current++ = ' ';
    }
  }

  *current = '\0';

  return joined;
}

static size_t environment_join_size(const char *const *environment)
{
  size_t joined_size = 1; // Count the null terminator.
  for (int i = 0; environment[i] != NULL; i++) {
    joined_size += strlen(environment[i]) + 1; // Count the null terminator.
  }

  return joined_size;
}

static char *environment_join(const char *const *environment)
{
  assert(environment);

  char *joined = calloc(environment_join_size(environment), sizeof(char));
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

static wchar_t *string_to_wstring(const char *string, size_t size)
{
  assert(string);
  // Overflow check although we really don't expect this to ever happen. This
  // makes the following casts to `int` safe.
  assert(size <= INT_MAX);

  // Determine wstring size (`MultiByteToWideChar` returns the required size if
  // its last two arguments are `NULL` and 0).
  int r = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, string, (int) size,
                              NULL, 0);
  if (r == 0) {
    return NULL;
  }

  // `MultiByteToWideChar` does not return negative values so the cast to
  // `size_t` is safe.
  wchar_t *wstring = calloc((size_t) r, sizeof(wchar_t));
  if (wstring == NULL) {
    return NULL;
  }

  // Now we pass our allocated string and its size as the last two arguments
  // instead of `NULL` and 0 which makes `MultiByteToWideChar` actually perform
  // the conversion.
  r = MultiByteToWideChar(CP_UTF8, 0, string, (int) size, wstring, r);
  if (r == 0) {
    free(wstring);
    return NULL;
  }

  return wstring;
}

static LPPROC_THREAD_ATTRIBUTE_LIST
handle_inherit_list_create(HANDLE *handles, size_t num_handles)
{
  assert(handles);

  // Get the required size for `attribute_list`.
  SIZE_T attribute_list_size = 0;
  if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attribute_list_size) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return NULL;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = malloc(attribute_list_size);
  if (!attribute_list) {
    return NULL;
  }

  if (!InitializeProcThreadAttributeList(attribute_list, 1, 0,
                                         &attribute_list_size)) {
    free(attribute_list);
    return NULL;
  }

  // Add the handles to be inherited to `attribute_list`.
  if (!UpdateProcThreadAttribute(attribute_list, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                 num_handles * sizeof(HANDLE), NULL, NULL)) {
    DeleteProcThreadAttributeList(attribute_list);
    return NULL;
  }

  return attribute_list;
}

REPROC_ERROR process_create(HANDLE *process,
                            const char *const *argv,
                            struct process_options options)
{
  assert(process);
  assert(argv);

  char *command_line = NULL;
  wchar_t *command_line_wstring = NULL;
  char *environment_line = NULL;
  wchar_t *environment_line_wstring = NULL;
  wchar_t *working_directory_wstring = NULL;
  LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = NULL;
  PROCESS_INFORMATION info = { HANDLE_INVALID, HANDLE_INVALID, 0, 0 };
  BOOL r = 0;

  // Child processes inherit the error mode of their parents. To avoid child
  // processes creating error dialogs we set our error mode to not create error
  // dialogs temporarily which is inherited by the child process.
  DWORD previous_error_mode = SetErrorMode(SEM_NOGPFAULTERRORBOX);

  // Join `argv` to a whitespace delimited string as required by
  // `CreateProcessW`.
  command_line = argv_join(argv);
  if (command_line == NULL) {
    goto cleanup;
  }

  // Convert UTF-8 to UTF-16 as required by `CreateProcessW`.
  command_line_wstring = string_to_wstring(command_line,
                                           strlen(command_line) + 1);
  if (command_line_wstring == NULL) {
    goto cleanup;
  }

  // Idem for `environment` if it isn't `NULL`.
  if (options.environment != NULL) {
    environment_line = environment_join(options.environment);
    if (environment_line == NULL) {
      goto cleanup;
    }

    size_t joined_size = environment_join_size(options.environment);
    environment_line_wstring = string_to_wstring(environment_line, joined_size);
    if (environment_line_wstring == NULL) {
      goto cleanup;
    }
  }

  // Idem for `working_directory` if it isn't `NULL`.
  if (options.working_directory != NULL) {
    size_t working_directory_size = strlen(options.working_directory) + 1;
    working_directory_wstring = string_to_wstring(options.working_directory,
                                                  working_directory_size);
    if (working_directory_wstring == NULL) {
      goto cleanup;
    }
  }

  // Windows Vista added the `STARTUPINFOEXW` structure in which we can put a
  // list of handles that should be inherited. Only these handles are inherited
  // by the child process. Other code in an application that calls
  // `CreateProcess` without passing a `STARTUPINFOEXW` struct containing the
  // handles it should inherit can still unintentionally inherit handles meant
  // for a reproc child process. See https://stackoverflow.com/a/2345126 for
  // more information.
  HANDLE inherit[3] = { options.redirect.in, options.redirect.out,
                        options.redirect.err };
  attribute_list = handle_inherit_list_create(inherit, ARRAY_SIZE(inherit));
  if (attribute_list == NULL) {
    goto cleanup;
  }

  STARTUPINFOEXW extended_startup_info = {
    .StartupInfo = { .cb = sizeof(extended_startup_info),
                     .dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW,
                     // `STARTF_USESTDHANDLES`
                     .hStdInput = options.redirect.in,
                     .hStdOutput = options.redirect.out,
                     .hStdError = options.redirect.err,
                     // `STARTF_USESHOWWINDOW`. Make sure the console window of
                     // the child process isn't visible. See
                     // https://github.com/DaanDeMeyer/reproc/issues/6 and
                     // https://github.com/DaanDeMeyer/reproc/pull/7 for more
                     // information.
                     .wShowWindow = SW_HIDE },
    .lpAttributeList = attribute_list
  };

  LPSTARTUPINFOW startup_info_address = &extended_startup_info.StartupInfo;

  r = CreateProcessW(NULL, command_line_wstring, &HANDLE_DO_NOT_INHERIT,
                     &HANDLE_DO_NOT_INHERIT, true, CREATION_FLAGS,
                     environment_line_wstring, working_directory_wstring,
                     startup_info_address, &info);
  if (r == 0) {
    goto cleanup;
  }

  if (job_reference_count == 0) {
    // `CreateJobObject` makes the handle uninheritable by default so we do not
    // pass `DO_NOT_INHERIT` here.
    job_object = CreateJobObjectA(NULL, NULL);
    if (job_object == NULL) {
      goto cleanup;
    }

    job_completion_port = CreateIoCompletionPort(HANDLE_INVALID, NULL, 0, 0);
    if (job_completion_port == NULL) {
      goto cleanup;
    }

    JOBOBJECT_ASSOCIATE_COMPLETION_PORT port_info = {
      .CompletionPort = job_completion_port
    };

    r = SetInformationJobObject(job_object,
                                JobObjectAssociateCompletionPortInformation,
                                &port_info, sizeof(port_info));
    if (r == 0) {
      goto cleanup;
    }
  }

  r = AssignProcessToJobObject(job_object, info.hProcess);
  if (r == 0) {
    goto cleanup;
  }

  r = ResumeThread(info.hThread) == 1;
  if (r == 0) {
    goto cleanup;
  }

  *process = info.hProcess;
  job_reference_count++;

cleanup:
  free(command_line);
  free(command_line_wstring);
  free(environment_line);
  free(environment_line_wstring);
  free(working_directory_wstring);
  DeleteProcThreadAttributeList(attribute_list);
  handle_destroy(info.hThread);

  SetErrorMode(previous_error_mode);

  if (r == 0) {
    if (info.hProcess != HANDLE_INVALID) {
      // The process is guaranteed to be suspended if we get here so terminating
      // it should be safe as it shouldn't be able to leak anything.
      TerminateProcess(info.hProcess, SIGKILL);
    }

    handle_destroy(info.hProcess);

    // `job_reference_count` is only incremented on success so we don't
    // decrement here.
    if (job_reference_count == 0) {
      job_completion_port = handle_destroy(job_completion_port);
      job_object = handle_destroy(job_object);
    }
  }

  return r == 0 ? REPROC_ERROR_SYSTEM : REPROC_SUCCESS;
}

REPROC_ERROR
process_wait(HANDLE *processes,
             unsigned int num_processes,
             unsigned int timeout,
             unsigned int *completed,
             int *exit_status)
{
  assert(completed);
  assert(exit_status);
  assert(completed);

  BOOL r = 0;

  for (unsigned int i = 0; i < num_processes; i++) {
    if (WaitForSingleObject(processes[i], 0) == WAIT_OBJECT_0) {
      DWORD status = 0;
      r = GetExitCodeProcess(processes[i], &status);
      if (r == 0) {
        return REPROC_ERROR_SYSTEM;
      }

      *completed = i;
      *exit_status = (int) status;

      return REPROC_SUCCESS;
    }
  }

  unsigned int remaining = timeout;
  long long completed_index = -1;

  while (completed_index == -1) {
    unsigned long long before = GetTickCount64();

    DWORD completion_code = 0;
    unsigned long long completion_key = 0;
    LPOVERLAPPED lpoverlapped = NULL;

    r = GetQueuedCompletionStatus(job_completion_port, &completion_code,
                                  &completion_key, &lpoverlapped,
                                  timeout == INFINITE ? timeout : remaining);
    if (r == 0) {
      return GetLastError() == WAIT_TIMEOUT ? REPROC_ERROR_WAIT_TIMEOUT
                                            : REPROC_ERROR_SYSTEM;
    }

    unsigned long long after = GetTickCount64();
    unsigned long long elapsed = after - before;

    remaining = elapsed >= remaining ? 0 : remaining - (unsigned int) elapsed;

    if (completion_code != JOB_OBJECT_MSG_EXIT_PROCESS &&
        completion_code != JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS) {
      continue;
    }

    uintptr_t pid = (uintptr_t) lpoverlapped;

    for (unsigned int i = 0; i < num_processes; i++) {
      if (GetProcessId(processes[i]) == pid) {
        completed_index = i;
        break;
      }
    }
  }

  DWORD status = 0;
  r = GetExitCodeProcess(processes[completed_index], &status);
  if (r == 0) {
    return REPROC_ERROR_SYSTEM;
  }

  *completed = (unsigned int) completed_index;
  *exit_status = (int) status;

  return REPROC_SUCCESS;
}

REPROC_ERROR process_terminate(HANDLE process)
{
  assert(process);

  // `GenerateConsoleCtrlEvent` can only be called on a process group. To call
  // `GenerateConsoleCtrlEvent` on a single child process it has to be put in
  // its own process group (which we did when starting the child process).
  return GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, GetProcessId(process)) == 0
             ? REPROC_ERROR_SYSTEM
             : REPROC_SUCCESS;
}

REPROC_ERROR process_kill(HANDLE process)
{
  assert(process);

  // We use 137 (`SIGKILL`) as the exit status because it is the same exit
  // status as a process that is stopped with the `SIGKILL` signal on POSIX
  // systems.
  return TerminateProcess(process, SIGKILL) == 0 ? REPROC_ERROR_SYSTEM
                                                 : REPROC_SUCCESS;
}

HANDLE process_destroy(HANDLE process)
{
  if (process == HANDLE_INVALID || process == NULL) {
    return HANDLE_INVALID;
  }

  assert(job_reference_count > 0);

  if (WaitForSingleObject(process, 0) == WAIT_TIMEOUT) {
    TerminateProcess(process, SIGKILL);
  }

  handle_destroy(process);

  job_reference_count--;
  if (job_reference_count == 0) {
    job_completion_port = handle_destroy(job_completion_port);
    job_object = handle_destroy(job_object);
  }

  return HANDLE_INVALID;
}

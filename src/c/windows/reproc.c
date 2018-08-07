#include "reproc/reproc.h"

#include "handle.h"
#include "pipe.h"
#include "process.h"
#include "string_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <windows.h>

static void reproc_destroy(reproc_type *process)
{
  assert(process);

  handle_close(&process->handle);

  handle_close(&process->parent_stdin);
  handle_close(&process->parent_stdout);
  handle_close(&process->parent_stderr);
}

REPROC_ERROR reproc_start(reproc_type *process, int argc,
                          const char *const *argv,
                          const char *working_directory)
{
  assert(process);

  assert(argc > 0);
  assert(argv);
  assert(argv[argc] == NULL);

  for (int i = 0; i < argc; i++) {
    assert(argv[i]);
  }

  // Predeclare every variable so we can use goto

  HANDLE child_stdin = NULL;
  HANDLE child_stdout = NULL;
  HANDLE child_stderr = NULL;

  char *command_line_string = NULL;
  wchar_t *command_line_wstring = NULL;
  wchar_t *working_directory_wstring = NULL;

  REPROC_ERROR error = REPROC_SUCCESS;

  // While we already make sure the child process only inherits the child pipe
  // handles using STARTUPINFOEXW (see process_utils.c) we still disable
  // inheritance of the parent pipe handles to lower the chance of other
  // CreateProcess calls (outside of this library) unintentionally inheriting
  // these handles.
  error = pipe_init(&child_stdin, &process->parent_stdin);
  if (error) { goto cleanup; }
  error = pipe_disable_inherit(process->parent_stdin);
  if (error) { goto cleanup; }

  error = pipe_init(&process->parent_stdout, &child_stdout);
  if (error) { goto cleanup; }
  error = pipe_disable_inherit(process->parent_stdout);
  if (error) { goto cleanup; }

  error = pipe_init(&process->parent_stderr, &child_stderr);
  if (error) { goto cleanup; }
  error = pipe_disable_inherit(process->parent_stderr);
  if (error) { goto cleanup; }

  // Join argv to whitespace delimited string as required by CreateProcess
  error = string_join(argv, argc, &command_line_string);
  if (error) { goto cleanup; }

  // Convert UTF-8 to UTF-16 as required by CreateProcessW
  error = string_to_wstring(command_line_string, &command_line_wstring);
  free(command_line_string); // Not needed anymore
  if (error) { goto cleanup; }

  // Do the same for the working directory string if one was provided
  error = working_directory
              ? string_to_wstring(working_directory, &working_directory_wstring)
              : REPROC_SUCCESS;
  if (error) { goto cleanup; }

  error = process_create(command_line_wstring, working_directory_wstring,
                         child_stdin, child_stdout, child_stderr, &process->id,
                         &process->handle);

cleanup:
  // The child process pipe endpoint handles are copied to the child process. We
  // don't need them anymore in the parent process so we close them.
  handle_close(&child_stdin);
  handle_close(&child_stdout);
  handle_close(&child_stderr);
  // Free allocated memory before returning possible error
  free(command_line_wstring);
  free(working_directory_wstring);

  if (error) {
    reproc_destroy(process);
    return error;
  }

  return REPROC_SUCCESS;
}

REPROC_ERROR reproc_write(reproc_type *process, const void *buffer,
                          unsigned int to_write, unsigned int *bytes_written)
{
  assert(process);
  assert(process->parent_stdin);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(process->parent_stdin, buffer, to_write, bytes_written);
}

void reproc_close(reproc_type *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
  case REPROC_STDIN: handle_close(&process->parent_stdin); break;
  case REPROC_STDOUT: handle_close(&process->parent_stdout); break;
  case REPROC_STDERR: handle_close(&process->parent_stderr); break;
  }
}

REPROC_ERROR reproc_read(reproc_type *process, REPROC_STREAM stream,
                         void *buffer, unsigned int size,
                         unsigned int *bytes_read)
{
  assert(process);
  assert(stream != REPROC_STDIN);
  assert(buffer);
  assert(bytes_read);

  switch (stream) {
  case REPROC_STDIN: break;
  case REPROC_STDOUT:
    return pipe_read(process->parent_stdout, buffer, size, bytes_read);
  case REPROC_STDERR:
    return pipe_read(process->parent_stderr, buffer, size, bytes_read);
  }

  // Only reachable when compiled without asserts
  return REPROC_UNKNOWN_ERROR;
}

REPROC_ERROR reproc_stop(reproc_type *process, int cleanup_flags,
                         unsigned int timeout, unsigned int *exit_status)
{
  assert(process);

  // Don't set to REPROC_SUCCESS so we can check if wait/terminate/kill
  // succeeded (sets error to REPROC_SUCCESS)
  REPROC_ERROR error = REPROC_WAIT_TIMEOUT;

  // We already did a 0 ms timeout check so we don't do it again
  if (cleanup_flags & REPROC_WAIT) {
    error = process_wait(process->handle, timeout, exit_status);
  }

  if (error != REPROC_WAIT_TIMEOUT) { goto cleanup; }

  if (cleanup_flags & REPROC_TERMINATE) {
    error = process_terminate(process->handle, process->id, timeout,
                              exit_status);
  }

  if (error != REPROC_WAIT_TIMEOUT) { goto cleanup; }

  if (cleanup_flags & REPROC_KILL) {
    error = process_kill(process->handle, timeout, exit_status);
  }

cleanup:
  reproc_destroy(process);

  return error;
}

unsigned int reproc_system_error(void) { return GetLastError(); }

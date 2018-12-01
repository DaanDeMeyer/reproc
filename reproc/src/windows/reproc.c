#include <reproc/reproc.h>

#include "handle.h"
#include "pipe.h"
#include "process.h"
#include "string_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <windows.h>

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

  // Predeclare every variable so we can use `goto`.

  HANDLE child_stdin = NULL;
  HANDLE child_stdout = NULL;
  HANDLE child_stderr = NULL;

  char *command_line_string = NULL;
  wchar_t *command_line_wstring = NULL;
  wchar_t *working_directory_wstring = NULL;

  REPROC_ERROR error = REPROC_SUCCESS;

  // While we already make sure the child process only inherits the child pipe
  // handles using `STARTUPINFOEXW` (see `process_utils.c`) we still disable
  // inheritance of the parent pipe handles to lower the chance of child
  // processes not created by reproc unintentionally inheriting these handles.
  error = pipe_init(&child_stdin, true, &process->in, false);
  if (error) { goto cleanup; }
  error = pipe_init(&process->out, false, &child_stdout, true);
  if (error) { goto cleanup; }
  error = pipe_init(&process->err, false, &child_stderr, true);
  if (error) { goto cleanup; }

  // Join `argv` to a whitespace delimited string as required by
  // `CreateProcessW`.
  error = string_join(argv, argc, &command_line_string);
  if (error) { goto cleanup; }

  // Convert UTF-8 to UTF-16 as required by `CreateProcessW`.
  error = string_to_wstring(command_line_string, &command_line_wstring);
  free(command_line_string); // Not needed anymore
  if (error) { goto cleanup; }

  // Do the same for `working_directory` if it isn't `NULL`.
  error = working_directory
              ? string_to_wstring(working_directory, &working_directory_wstring)
              : REPROC_SUCCESS;
  if (error) { goto cleanup; }

  struct process_options options = {
    .working_directory = working_directory_wstring,
    .stdin_handle = child_stdin,
    .stdout_handle = child_stdout,
    .stderr_handle = child_stderr
  };

  error = process_create(command_line_wstring, &options, &process->id,
                         &process->handle);

cleanup:
   // Eithera n error has ocurred or the child pipe endpoints have been copied to
  // the stdin/stdout/stderr streams of the child process. Either way they can
  // be safely closed in the parent process.
  handle_close(&child_stdin);
  handle_close(&child_stdout);
  handle_close(&child_stderr);

  free(command_line_wstring);
  free(working_directory_wstring);

  if (error) { reproc_destroy(process); }

  return error;
}

REPROC_ERROR reproc_read(reproc_type *process, REPROC_STREAM stream,
                         void *buffer, unsigned int size,
                         unsigned int *bytes_read)
{
  assert(process);
  assert(stream != REPROC_IN);
  assert(buffer);
  assert(bytes_read);

  switch (stream) {
  case REPROC_IN: break;
  case REPROC_OUT: return pipe_read(process->out, buffer, size, bytes_read);
  case REPROC_ERR: return pipe_read(process->err, buffer, size, bytes_read);
  }

  assert(0);
  return REPROC_UNKNOWN_ERROR;
}

REPROC_ERROR reproc_write(reproc_type *process, const void *buffer,
                          unsigned int to_write, unsigned int *bytes_written)
{
  assert(process);
  assert(process->in);
  assert(buffer);
  assert(bytes_written);

  return pipe_write(process->in, buffer, to_write, bytes_written);
}

void reproc_close(reproc_type *process, REPROC_STREAM stream)
{
  assert(process);

  switch (stream) {
  case REPROC_IN: handle_close(&process->in); return;
  case REPROC_OUT: handle_close(&process->out); return;
  case REPROC_ERR: handle_close(&process->err); return;
  }

  assert(0);
}

REPROC_ERROR reproc_wait(reproc_type *process, unsigned int timeout,
                         unsigned int *exit_status)
{
  assert(process);

  return process_wait(process->handle, timeout, exit_status);
}

REPROC_ERROR reproc_terminate(reproc_type *process)
{
  assert(process);

  return process_terminate(process->id);
}

REPROC_ERROR reproc_kill(reproc_type *process)
{
  assert(process);

  return process_kill(process->handle);
}

void reproc_destroy(reproc_type *process)
{
  assert(process);

  handle_close(&process->handle);

  handle_close(&process->in);
  handle_close(&process->out);
  handle_close(&process->err);
}

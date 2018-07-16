#include <process-lib/process.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*! Uses the process-lib C API to print CMake's help page */
int main(void)
{
  // User is responsible for allocating memory for process_type
  process_type *process = malloc(process_size());
  if (!process) { return 1; }

  PROCESS_LIB_ERROR error = PROCESS_LIB_SUCCESS;

  // Always call process_init after allocating memory for process_type to
  // initialize it
  error = process_init(process);
  if (error) { return error; }

  // process_start takes argc and argv as passed to the main function. Note that
  // argc does not include the final NULL element of the array.
  int argc = 2;
  const char *argv[3] = { "cmake", "--help", NULL };

  // process_start takes argv, argc and the working directory of the child
  // process. If the working directory is NULL the working directory of the
  // parent process is used.
  error = process_start(process, argc, argv, NULL);
  if (error) { return error; }

  char buffer[1024];
  // One less than actual size to always have space left for a null terminator
  unsigned int buffer_size = 1023;

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. The
  // while loop keeps running until an error occurs in process_read (the child
  // process closing its output stream is also reported as an error).
  while (true) {
    unsigned int bytes_read = 0;
    error = process_read(process, buffer, buffer_size, &bytes_read);
    if (error) { break; }

    buffer[bytes_read] = '\0';
    fprintf(stdout, "%s", buffer);
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of another error
  if (error != PROCESS_LIB_STREAM_CLOSED) { return error; }

  // Wait for the process to exit. This should always be done since some systems
  // don't clean up system resources allocated to a child process until the
  // parent process waits for it.
  error = process_wait(process, PROCESS_LIB_INFINITE);
  if (error) { return error; }

  // Get the exit status (returns error if the process is still running)
  int exit_status = 0;
  error = process_exit_status(process, &exit_status);
  if (error) { return error; }

  // Release all remaining resources related to the child process
  error = process_destroy(process);
  if (error) { return error; }

  // Free the process memory itself
  free(process);

  return exit_status;
}

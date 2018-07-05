#include <process.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  PROCESS_LIB_ERROR error;

  // User is responsible for allocating memory for process_type
  process_type *process = malloc(process_size());

  // Always call process_init after allocating memory for process_type to
  // initialize it
  error = process_init(process);
  if (error) { return 1; }

  // process_start takes argc and argv as passed to the main function. Note that
  // argc does not include the final NULL element of the array.
  int argc = 2;
  const char *argv[3] = {"cmake", "--help", NULL};

  // process_start takes argv, argc and the working directory of the child
  // process. If the working directory is NULL the working directory of the
  // parent process is used.
  error = process_start(process, argc, argv, NULL);
  if (error) { return 1; }

  char buffer[1024];
  unsigned int buffer_size = 1024;
  unsigned int actual;

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. Note
  // that since we're reading strings from the output we pass buffer_size - 1 to
  // the library which leaves space in the buffer for a null terminator at the
  // end. The while loop keeps running until an error occurs in process_read
  // (the child process closing its output stream is also counted as an error).
  while (true) {
    error = process_read(process, buffer, buffer_size - 1, &actual);
    if (error) { break; }

    buffer[actual] = '\0';
    fprintf(stdout, "%s", buffer);
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of another error
  if (error != PROCESS_LIB_STREAM_CLOSED) { return 1; }

  // Wait for the process to exit
  error = process_wait(process, PROCESS_LIB_INFINITE);
  if (error) { return 1; }

  // Get the exit status (returns error if the process is still running)
  int exit_status;
  error = process_exit_status(process, &exit_status);
  if (error) { return 1; }

  // Free all resources stored in process
  error = process_free(process);
  if (error) { return 1; }

  // Free the process memory itself
  free(process);

  return exit_status;
}

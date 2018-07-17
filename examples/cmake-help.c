#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*! Uses the reproc C API to print CMake's help page */
int main(void)
{
  // User is responsible for allocating memory for reproc_type
  reproc_type *reproc = malloc(reproc_size());
  if (!reproc) { return 1; }

  REPROC_ERROR error = REPROC_SUCCESS;

  // Always call reproc_init after allocating memory for reproc_type to
  // initialize it
  error = reproc_init(reproc);
  if (error) { return error; }

  // reproc_start takes argc and argv as passed to the main function. Note that
  // argc does not include the final NULL element of the array.
  int argc = 2;
  const char *argv[3] = { "cmake", "--help", NULL };

  // reproc_start takes argv, argc and the working directory of the child
  // process. If the working directory is NULL the working directory of the
  // parent process is used.
  error = reproc_start(reproc, argc, argv, NULL);
  if (error) { return error; }

  char buffer[1024];
  // One less than actual size to always have space left for a null terminator
  unsigned int buffer_size = 1023;

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. The
  // while loop keeps running until an error occurs in reproc_read (the child
  // process closing its output stream is also reported as an error).
  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(reproc, buffer, buffer_size, &bytes_read);
    if (error) { break; }

    buffer[bytes_read] = '\0';
    fprintf(stdout, "%s", buffer);
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of another error
  if (error != REPROC_STREAM_CLOSED) { return error; }

  // Wait for the process to exit. This should always be done since some systems
  // don't clean up system resources allocated to a child process until the
  // parent process waits for it.
  error = reproc_wait(reproc, REPROC_INFINITE);
  if (error) { return error; }

  // Get the exit status (returns error if the child process is still running)
  int exit_status = 0;
  error = reproc_exit_status(reproc, &exit_status);
  if (error) { return error; }

  // Release all remaining resources related to the child process
  error = reproc_destroy(reproc);
  if (error) { return error; }

  // Free the process memory itself
  free(reproc);

  return exit_status;
}

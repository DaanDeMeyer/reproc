#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

/*! Uses the reproc C API to print CMake's help page */
int main(void)
{
  reproc_type git_status;

  // reproc_start imposes the same restrictions on argc and argv as the regular
  // main function of C and C++ applications.
  int argc = 2;
  const char *argv[3] = { "git", "status", NULL };

  REPROC_ERROR error = REPROC_SUCCESS;

  // reproc_start takes argc, argv and the working directory of the child
  // process. If the working directory is NULL the working directory of the
  // parent process is used.
  error = reproc_start(&git_status, argc, argv, NULL);
  if (error == REPROC_FILE_NOT_FOUND) {
    fprintf(stderr, "%s",
            "git not found. Make sure it's available from the PATH");
    return 1;
  } else if (error) {
    return (int) error;
  }

  // Start with an empty string
  size_t size = 0;
  char *output = malloc(1);
  if (!output) { return 1; }
  output[0] = '\0';

  char buffer[BUFFER_SIZE];

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. The
  // while loop keeps running until an error occurs in reproc_read (the child
  // process closing its output stream is also reported as an error).
  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(&git_status, REPROC_STDOUT, buffer, BUFFER_SIZE,
                        &bytes_read);
    if (error) { break; }

    // +1 to leave space for null terminator
    char *realloc_result = realloc(output, size + bytes_read + 1);
    if (!realloc_result) {
      free(output);
      return 1;
    }
    output = realloc_result;

    memcpy(output + size, buffer, bytes_read);
    size += bytes_read;
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of another error
  if (error != REPROC_STREAM_CLOSED) {
    free(output);
    return (int) error;
  }

  output[size] = '\0';
  printf("%s", output);
  free(output);

  // Wait for the process to exit. This should always be done since some systems
  // don't clean up system resources allocated to a child process until the
  // parent process waits for it.
  unsigned int exit_status = 0;
  error = reproc_wait(&git_status, REPROC_INFINITE, &exit_status);
  if (error) { return (int) error; }

  // Release all remaining resources related to the child process
  reproc_destroy(&git_status);

  return (int) exit_status;
}

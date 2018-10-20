/*! \example git-status.c */

#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int fail(REPROC_ERROR error)
{
  fprintf(stderr, "%s\n", reproc_error_to_string(error));
  return (int) error;
}

// Uses the reproc C API to print the output of git status.
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
    fprintf(stderr, "%s\n",
            "git not found. Make sure it's available from the PATH");
    return 1;
  }

  if (error) { return fail(error); }

  // Close the stdin stream since we're not going to write any input to git.
  // While the example works perfectly without closing stdin we do it here to
  // show how reproc_close works.
  reproc_close(&git_status, REPROC_IN);

  // Start with an empty string
  size_t output_length = 0;
  char *output = malloc(1);
  if (!output) { goto cleanup; }
  output[0] = '\0';

  char buffer[BUFFER_SIZE];

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. The
  // while loop keeps running until an error occurs in reproc_read (the child
  // process closing its output stream is also reported as an error).
  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(&git_status, REPROC_OUT, buffer, BUFFER_SIZE,
                        &bytes_read);
    if (error) { break; }

    // Add 1 to size to leave space for a null terminator.
    char *realloc_result = realloc(output, output_length + bytes_read + 1);
    if (!realloc_result) {
      free(output);
      goto cleanup;
    }
    output = realloc_result;

    memcpy(output + output_length, buffer, bytes_read);
    output_length += bytes_read;
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of another error.
  if (error != REPROC_STREAM_CLOSED) {
    free(output);
    goto cleanup;
  }

  output[output_length] = '\0';
  printf("%s", output);
  free(output);

// Add a ; because declaration directly after label is illegal in C.
cleanup:;
  // Wait for the process to exit. This should always be done since some systems
  // (POSIX) don't clean up system resources allocated to a child process until
  // the parent process explicitly waits for it after it has exited.
  unsigned int exit_status = 0;
  error = reproc_stop(&git_status, REPROC_WAIT, REPROC_INFINITE, &exit_status);

  if (error) { return fail(error); }

  return (int) exit_status;
}

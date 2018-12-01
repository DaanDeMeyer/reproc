#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

int fail(REPROC_ERROR error)
{
  fprintf(stderr, "%s\n", reproc_strerror(error));
  return (int) error;
}

// Uses reproc to print the output of git status.
int main(void)
{
  // reproc_type stores necessary information between calls to the reproc C API.
  reproc_type git_status;

  /* reproc_start imposes the same restrictions on argc and argv as the regular
  main function of C and C++ applications. argv is required to end with a NULL
  element and argc should be one less than the total amount of elements in argv
  (it doesn't count the NULL). */
  int argc = 2;
  const char *argv[3] = { "git", "status", NULL };

  // Most of reproc's API functions return a value from REPROC_ERROR to indicate
  // if an error occurred. If no error occurred REPROC_SUCCESS is returned.
  REPROC_ERROR error = REPROC_SUCCESS;

  /* reproc_start takes a child process instance (reproc_type), argc, argv and
  the working directory of the child process. If the working directory is NULL
  the working directory of the parent process is used. */
  error = reproc_start(&git_status, argc, argv, NULL);

  /* reproc exposes a single error enum REPROC_ERROR which contains values for
  all system errors that reproc checks for explicitly. If an unknown error
  occurs reproc's functions will return REPROC_UNKNOWN_ERROR. You can get the
  actual system error using the reproc_system_error function. */
  if (error == REPROC_FILE_NOT_FOUND) {
    fprintf(stderr, "%s\n",
            "git not found. Make sure it's available from the PATH.");
    return 1;
  }

  // Shorthand for `if (error != REPROC_SUCCESS)`.
  if (error) { return fail(error); }

  /* Close the stdin stream since we're not going to write any input to git.
  While the example works perfectly without closing stdin we do it here to
  show how reproc_close works. */
  reproc_close(&git_status, REPROC_IN);

  // Start with an empty string (only space for the null terminator is
  // allocated).
  size_t output_length = 0;
  char *output = malloc(1);
  if (!output) { goto cleanup; }
  output[0] = '\0';

  char buffer[BUFFER_SIZE];

  /* Read the entire output of the child process. I've found this pattern to be
  the most readable when reading the entire output of a child process. The while
  loop keeps running until an error occurs in reproc_read (the child process
  closing its output stream is also reported as an error). */
  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(&git_status, REPROC_OUT, buffer, BUFFER_SIZE,
                        &bytes_read);
    if (error) { break; }

    /* Increase size of result to make sure it can hold the new output. This is
    definitely not the most performant way to grow a buffer so keep that in
    mind. Add 1 to size to leave space for the null terminator which isn't
    included in output_length. */
    char *realloc_result = realloc(output, output_length + bytes_read + 1);
    if (!realloc_result) { goto cleanup; }
    output = realloc_result;

    // Copy new data into result buffer
    memcpy(output + output_length, buffer, bytes_read);
    output_length += bytes_read;
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of any other error.
  if (error != REPROC_STREAM_CLOSED) { goto cleanup; }

  output[output_length] = '\0';
  printf("%s", output);

cleanup:
  free(output);

  /* Wait for the process to exit. This should always be done since some systems
  (POSIX) don't clean up system resources allocated to a child process until the
  parent process explicitly waits for it after it has exited. */
  unsigned int exit_status = 0;
  error = reproc_wait(&git_status, REPROC_INFINITE, &exit_status);

  // git status will always exit on its own so calling reproc_terminate or
  // reproc_kill is not necessary.

  /* Clean up the resources allocated to the child process. Calling this
  function before calling reproc_wait (or reproc_terminate/reproc_kill)
  successfully will result in a resource leak on POSIX systems. See the Gotcha's
  section in the readme for more information. */
  reproc_destroy(&git_status);

  if (error) { return fail(error); }

  return (int) exit_status;
}

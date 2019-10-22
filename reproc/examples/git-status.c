#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

static int fail(REPROC_ERROR error)
{
  fprintf(stderr, "%s\n", reproc_error_string(error));
  return (int) error;
}

// Uses reproc to print the output of git status.
int main(void)
{
  // `reproc_t` stores necessary information between calls to reproc's API.
  reproc_t git_status;

  // `argv` must start with the name of the program to execute and must end with
  // a `NULL` value.
  const char *argv[3] = { "git", "status", NULL };

  // Most of reproc's API functions return a value from `REPROC_ERROR` to
  // indicate if an error occurred. If no error occurred `REPROC_SUCCESS` is
  // returned.
  REPROC_ERROR error = REPROC_SUCCESS;

  // `reproc_start` takes a child process instance (`reproc_t`), argv and
  // optionally the working directory and the environment of the child process.
  // If the working directory is `NULL` the working directory of the parent
  // process is used. If the environment is `NULL`, the environment of the
  // parent process is used.
  error = reproc_start(&git_status, argv, NULL, NULL);

  // reproc exposes a single error enum `REPROC_ERROR` which contains errors
  // specific to reproc and `REPROC_ERROR_SYSTEM` to indicate a system error
  // occurred. You can get the actual system error using the
  // `reproc_error_system` function.

  // Shorthand for `if (error != REPROC_SUCCESS)`.
  if (error) {
    return fail(error);
  }

  // Close the stdin stream since we're not going to write any input to git.
  // While the example works perfectly without closing stdin we do it here to
  // show how `reproc_close` works.
  reproc_close(&git_status, REPROC_STREAM_IN);

  // Start with an empty string (only space for the null terminator is
  // allocated).
  char *output = NULL;
  size_t output_size = 0;

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. The
  // while loop keeps running until an error occurs in `reproc_read` (the child
  // process closing its output stream is also reported as an error).
  while (true) {
    uint8_t buffer[BUFFER_SIZE];
    unsigned int bytes_read = 0;

    // `reproc_read` takes an optional pointer to a `REPROC_STREAM` and sets its
    // value to the stream that it read from. As we're going to put both the
    // stdout and stderr output in the same string, we pass `NULL` since we
    // don't need to know which stream was read from.
    error = reproc_read(&git_status, NULL, buffer, BUFFER_SIZE, &bytes_read);
    if (error) {
      break;
    }

    // Increase the size of `output` to make sure it can hold the new output.
    // This is definitely not the most performant way to grow a buffer so keep
    // that in mind. Add 1 to size to leave space for the null terminator which
    // isn't included in `output_size`.
    char *realloc_result = realloc(output, output_size + bytes_read + 1);
    if (!realloc_result) {
      goto cleanup;
    } else {
      output = realloc_result;
    }

    // Copy new data into `output`.
    memcpy(output + output_size, buffer, bytes_read);
    output[output_size + bytes_read] = '\0';
    output_size += bytes_read;
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of any other error.
  if (error != REPROC_ERROR_STREAM_CLOSED) {
    goto cleanup;
  }

  printf("%s", output);

cleanup:
  free(output);

  // Wait for the process to exit. This should always be done since some systems
  // (POSIX) don't clean up system resources allocated to a child process until
  // the parent process explicitly waits for it after it has exited.
  error = reproc_wait(&git_status, REPROC_INFINITE);

  // git status will always exit on its own so calling reproc_terminate or
  // reproc_kill is not necessary.

  // Clean up the resources allocated to the child process. Calling this
  // function before calling `reproc_wait` (or `reproc_terminate`/`reproc_kill`)
  // successfully will result in a resource leak on POSIX systems. See the
  // Gotchas section in the readme for more information.
  reproc_destroy(&git_status);

  if (error) {
    return fail(error);
  }

  return (int) reproc_exit_status(&git_status);
}

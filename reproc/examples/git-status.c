#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Uses reproc to print the output of git status.
int main(void)
{
  // `reproc_t` stores necessary information between calls to reproc's API.
  reproc_t *process = reproc_new();

  int exit_status = -1;
  char *output = NULL;
  size_t output_size = 0;

  // `argv` must start with the name (or path) of the program to execute and
  // must end with a `NULL` value.
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
  error = reproc_start(process, argv, (reproc_options){ 0 });

  // reproc exposes a single error enum `REPROC_ERROR` which contains errors
  // specific to reproc and `REPROC_ERROR_SYSTEM` to indicate a system error
  // occurred. You can get the actual system error using the
  // `reproc_error_system` function.

  // Shorthand for `if (error != REPROC_SUCCESS)`.
  if (error) {
    goto cleanup;
  }

  // Close the stdin stream since we're not going to write any input to git.
  // While the example works perfectly without closing stdin we do it here to
  // show how `reproc_close` works.
  reproc_close(process, REPROC_STREAM_IN);

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. The
  // while loop keeps running until an error occurs in `reproc_read` (the child
  // process closing its output stream is also reported as an error).
  while (true) {
    uint8_t buffer[4096];
    unsigned int bytes_read = 0;

    // `reproc_read` takes an optional pointer to a `REPROC_STREAM` and sets its
    // value to the stream that it read from. As we're going to put both the
    // stdout and stderr output in the same string, we pass `NULL` since we
    // don't need to know which stream was read from.
    error = reproc_read(process, NULL, buffer, sizeof(buffer), &bytes_read);
    if (error) {
      break;
    }

    // Increase the size of `output` to make sure it can hold the new output.
    // This is definitely not the most performant way to grow a buffer so keep
    // that in mind. Add 1 to size to leave space for the null terminator which
    // isn't included in `output_size`.
    char *realloc_result = realloc(output, output_size + bytes_read + 1);
    if (!realloc_result) {
      fprintf(stderr, "Failed to allocate memory for output\n");
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

  // Wait for the process to exit. This should always be done since some systems
  // (POSIX) don't clean up system resources allocated to a child process until
  // the parent process explicitly waits for it after it has exited.
  error = reproc_wait(process, REPROC_INFINITE);
  if (error) {
    goto cleanup;
  }

  exit_status = reproc_exit_status(process);

cleanup:
  free(output);

  // Clean up all the resources allocated to the child process (including the
  // memory allocated by `reproc_new`). Unless custom stop actions are passed to
  // `reproc_start`, `reproc_destroy` will first wait indefinitely for the child
  // process to exit.
  reproc_destroy(process);

  if (error) {
    fprintf(stderr, "%s\n", reproc_error_string(error));
    exit_status = (int) error;
  }

  return exit_status < 0 ? (int) reproc_error_system() : exit_status;
}

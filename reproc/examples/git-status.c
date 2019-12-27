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

  // `argv` must start with the name (or path) of the program to execute and
  // must end with a `NULL` value.
  const char *argv[3] = { "git", "status", NULL };

  char *output = NULL;
  size_t size = 0;
  int r = -1;

  // `reproc_start` takes a child process instance (`reproc_t`), argv and
  // a set of options including the working directory and environment of the
  // child process. If the working directory is `NULL` the working directory of
  // the parent process is used. If the environment is `NULL`, the environment
  // of the parent process is used.
  r = reproc_start(process, argv, (reproc_options){ 0 });

  // On failure, reproc's API functions return a negative `errno` (POSIX) or
  // `GetLastError` (Windows) style error code. To check against common error
  // codes, reproc provides cross platform constants such as
  // `REPROC_ERROR_STREAM_CLOSED` and `REPROC_ERROR_WAIT_TIMEOUT`.
  if (r < 0) {
    goto cleanup;
  }

  // Close the stdin stream since we're not going to write any input to git.
  // While the example works perfectly without closing stdin we do it here to
  // show how `reproc_close` works.
  r = reproc_close(process, REPROC_STREAM_IN);
  if (r < 0) {
    goto cleanup;
  }

  // Read the entire output of the child process. I've found this pattern to be
  // the most readable when reading the entire output of a child process. The
  // while loop keeps running until an error occurs in `reproc_read` (the child
  // process closing its output stream is also reported as an error).
  while (true) {
    // `reproc_read` takes an optional pointer to a `REPROC_STREAM` and sets its
    // value to the stream that it read from. As we're going to put both the
    // stdout and stderr output in the same string, we pass `NULL` since we
    // don't need to know which stream was read from.
    uint8_t buffer[4096];
    r = reproc_read(process, NULL, buffer, sizeof(buffer));
    if (r < 0) {
      break;
    }

    // On success, `reproc_read` returns the amount of bytes read.
    size_t bytes_read = (size_t) r;

    // Increase the size of `output` to make sure it can hold the new output.
    // This is definitely not the most performant way to grow a buffer so keep
    // that in mind. Add 1 to size to leave space for the null terminator which
    // isn't included in `output_size`.
    char *result = realloc(output, size + bytes_read + 1);
    if (result == NULL) {
      fprintf(stderr, "Failed to allocate memory for output\n");
      goto cleanup;
    } else {
      output = result;
    }

    // Copy new data into `output`.
    memcpy(output + size, buffer, bytes_read);
    output[size + bytes_read] = '\0';
    size += bytes_read;
  }

  // Check that the while loop stopped because the output stream of the child
  // process was closed and not because of any other error.
  if (r != REPROC_ERROR_STREAM_CLOSED) {
    goto cleanup;
  }

  printf("%s", output);

  // Wait for the process to exit. This should always be done since some systems
  // (POSIX) don't clean up system resources allocated to a child process until
  // the parent process explicitly waits for it after it has exited.
  r = reproc_wait(process, REPROC_INFINITE);
  if (r < 0) {
    goto cleanup;
  }

  r = reproc_exit_status(process);

cleanup:
  free(output);

  // Clean up all the resources allocated to the child process (including the
  // memory allocated by `reproc_new`). Unless custom stop actions are passed to
  // `reproc_start`, `reproc_destroy` will first wait indefinitely for the child
  // process to exit.
  reproc_destroy(process);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_error_string(r));
  }

  return abs(r);
}

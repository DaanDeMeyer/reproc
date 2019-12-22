#include <reproc/reproc.h>

#include <stdio.h>

// Forwards the provided arguments to `reproc_start` which starts a child
// process that inherits the standard streams of the parent process. It will
// read its input from the stdin of this process and write its output to the
// stdout/stderr of this process.
int main(int argc, const char *argv[])
{
  if (argc <= 1) {
    fprintf(stderr,
            "No arguments provided. Example usage: ./inherit cmake --help");
    return 1;
  }

  reproc_t *process = reproc_new();
  int exit_status = -1;

  reproc_options options = { .redirect = { .in = REPROC_REDIRECT_INHERIT,
                                           .out = REPROC_REDIRECT_INHERIT,
                                           .err = REPROC_REDIRECT_INHERIT } };

  REPROC_ERROR error = reproc_start(process, argv + 1, options);
  if (error) {
    goto cleanup;
  }

  error = reproc_wait(process, REPROC_INFINITE);
  if (error) {
    goto cleanup;
  }

  exit_status = reproc_exit_status(process);

cleanup:
  reproc_destroy(process);

  if (error) {
    fprintf(stderr, "%s\n", reproc_error_string(error));
    exit_status = (int) error;
  }

  return exit_status < 0 ? (int) reproc_error_system() : exit_status;
}

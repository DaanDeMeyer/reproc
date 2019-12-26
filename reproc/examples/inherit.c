#include <reproc/reproc.h>

#include <stdio.h>
#include <stdlib.h>

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

  reproc_options options = { .redirect = { .in = REPROC_REDIRECT_INHERIT,
                                           .out = REPROC_REDIRECT_INHERIT,
                                           .err = REPROC_REDIRECT_INHERIT } };
  int r = -1;

  r = reproc_start(process, argv + 1, options);
  if (r < 0) {
    goto cleanup;
  }

  r = reproc_wait(process, REPROC_INFINITE);
  if (r < 0) {
    goto cleanup;
  }

  r = reproc_exit_status(process);

cleanup:
  reproc_destroy(process);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_error_string(r));
  }

  return abs(r);
}

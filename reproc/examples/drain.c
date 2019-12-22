#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Shows the output of git --help using `reproc_drain`. We only explain the
// specifics of `reproc_drain` here, see the git-status example for general
// information on how to use reproc.
int main(void)
{
  reproc_t *process = reproc_new();

  char *output = NULL;
  int exit_status = -1;

  const char *argv[3] = { "git", "--help", NULL };

  REPROC_ERROR error = REPROC_SUCCESS;

  error = reproc_start(process, argv, (reproc_options) { 0 });
  if (error) {
    goto cleanup;
  }

  reproc_close(process, REPROC_STREAM_IN);

  // A sink function receives a single context parameter. For
  // `reproc_sink_string` we require a `char **` with its value set to `NULL` to
  // be passed to `reproc_drain` along with `reproc_sink_string`. If a sink
  // function needs more than one parameter, simply store the parameters in a
  // struct and pass the address of the struct as the `context` parameter.
  error = reproc_drain(process, reproc_sink_string, &output);
  if (error) {
    goto cleanup;
  }

  if (output == NULL) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    goto cleanup;
  }

  printf("%s", output);

  error = reproc_wait(process, REPROC_INFINITE);
  if (error) {
    goto cleanup;
  }

  exit_status = reproc_exit_status(process);

cleanup:
  // `output` always points to valid memory or is set to `NULL` by
  // `reproc_sink_string` so it's always safe to call `free` on it.
  free(output);

  reproc_destroy(process);

  if (error) {
    fprintf(stderr, "%s\n", reproc_error_string(error));
    exit_status = (int) error;
  }

  return exit_status < 0 ? (int) reproc_error_system() : exit_status;
}

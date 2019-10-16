#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(REPROC_ERROR error)
{
  fprintf(stderr, "%s\n", reproc_error_string(error));
  return (int) error;
}

// Shows the output of git --help using `reproc_drain`. We only explain the
// specifics of `reproc_drain` here, see the git-status example for general
// information on how to use reproc.
int main(void)
{
  reproc_t git_help;

  const char *argv[3] = { "git", "--help", NULL };

  REPROC_ERROR error = REPROC_SUCCESS;

  error = reproc_start(&git_help, argv, NULL, NULL);
  if (error) {
    return fail(error);
  }

  reproc_close(&git_help, REPROC_STREAM_IN);

  // A sink function receives a single context parameter. For
  // `reproc_sink_string` we require a `char **` with its value set to `NULL` to
  // be passed to `reproc_drain` along with `reproc_sink_string`. If a sink
  // function needs more than one parameter, simply store the parameters in a
  // struct and pass the address of the struct as the `context` parameter.
  char *output = NULL;
  error = reproc_drain(&git_help, REPROC_STREAM_OUT, reproc_sink_string,
                       &output);
  if (error) {
    goto cleanup;
  }

  if (output == NULL) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    goto cleanup;
  }

  printf("%s", output);

cleanup:
  // `output` always points to valid memory or is set to `NULL` by
  // `reproc_sink_string` so it's always safe to call `free` on it.
  free(output);

  error = reproc_wait(&git_help, REPROC_INFINITE);

  reproc_destroy(&git_help);

  if (error) {
    return fail(error);
  }

  return (int) reproc_exit_status(&git_help);
}

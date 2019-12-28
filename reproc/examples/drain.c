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
  reproc_t *process = NULL;
  char *output = NULL;
  int r = REPROC_ENOMEM;

  process = reproc_new();
  if (process == NULL) {
    goto cleanup;
  }

  const char *argv[3] = { "git", "--help", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  if (r < 0) {
    goto cleanup;
  }

  r = reproc_close(process, REPROC_STREAM_IN);
  if (r < 0) {
    goto cleanup;
  }

  // A sink function receives a single context parameter. `reproc_sink_string`
  // requires a `char **` with its value set to `NULL` to be passed to
  // `reproc_drain` along with `reproc_sink_string`. If a sink function needs
  // more than one parameter, simply store the parameters in a struct and pass
  // the address of the struct as the `context` parameter.
  r = reproc_drain(process, reproc_sink_string, &output);
  if (r < 0) {
    goto cleanup;
  }

  if (output == NULL) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    goto cleanup;
  }

  printf("%s", output);

  r = reproc_wait(process, REPROC_INFINITE);
  if (r < 0) {
    goto cleanup;
  }

cleanup:
  // `output` always points to valid memory or is set to `NULL` by
  // `reproc_sink_string` so it's always safe to call `free` on it.
  free(output);

  reproc_destroy(process);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A sink function receives a single context parameter. For `string_sink` we
// require a `char **` with its value set to `NULL` to be passed to
// `reproc_drain`. If more than one parameter is needed, simply store the
// parameters in a struct and pass the address of the struct as the `context`
// parameter.

bool string_sink(const uint8_t *buffer, unsigned int size, void *context)
{
  // `context` is required to be of type `char **` when passing `string_sink` to
  // `reproc_drain` so we cast `context` to the correct type.
  char **string = (char **) context;
  size_t string_size = *string == NULL ? 0 : strlen(*string);

  char *realloc_result = realloc(*string, string_size + size + 1);
  if (!realloc_result) {
    free(*string);
    *string = NULL;
    return false;
  } else {
    *string = realloc_result;
  }

  if (buffer != NULL) {
    memcpy(*string + string_size, buffer, size);
  }

  (*string)[string_size + size] = '\0';

  return true;
}

int fail(REPROC_ERROR error)
{
  fprintf(stderr, "%s\n", reproc_strerror(error));
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

  // Passing the address of `output` to `reproc_drain` as `string_sink`'s
  // context is safe as it a `char **` with its value set to `NULL`.
  char *output = NULL;
  error = reproc_drain(&git_help, REPROC_STREAM_OUT, string_sink, &output);
  if (error) {
    goto cleanup;
  }

  if (output == NULL) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    goto cleanup;
  }

  printf("%s", output);

cleanup:
  free(output);

  error = reproc_wait(&git_help, REPROC_INFINITE);

  reproc_destroy(&git_help);

  if (error) {
    return fail(error);
  }

  return (int) reproc_exit_status(&git_help);
}

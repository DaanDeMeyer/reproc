#include <reproc/reproc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A sink function receives a single context parameter. Because we need more
// than parameter in `string_sink` we take arguments via a struct. Before
// calling `reproc_drain`, all parameters should be filled in a
// `string_sink_context` which should then be passed along with `string_sink` as
// an argument to `reproc_drain`.

// We store the addresses so we can directly use the modified values when
// `reproc_drain` returns. Another option is to store the values directly and
// reassign them when `reproc_drain` returns.
typedef struct {
  char **buffer;
  size_t *length;
} string_sink_context;

bool string_sink(const uint8_t *buffer, unsigned int size, void *context)
{
  // `context` is required to be of type `string_sink_context` when passing
  // `string_sink` to `reproc_drain` so we cast `context` to the correct type.
  string_sink_context *ctx = (string_sink_context *) context;

  char *realloc_result = realloc(*ctx->buffer, *ctx->length + size + 1);
  if (!realloc_result) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    return false;
  } else {
    *ctx->buffer = realloc_result;
  }

  memcpy(*ctx->buffer + *ctx->length, buffer, size);
  *ctx->length += size;

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

  size_t output_length = 0;
  char *output = malloc(1);
  if (!output) {
    goto cleanup;
  }
  output[0] = '\0';

  // Function pointers passed to reproc_drain receive a single context argument
  // which we provide when calling `reproc_drain` Because we need access to both
  // `output` and `output_length` inside `string_sink`, we store both in a
  // temporary `string_sink_context` struct which we pass as a context to
  // `reproc_drain`.
  string_sink_context context = { .buffer = &output, .length = &output_length };

  error = reproc_drain(&git_help, REPROC_STREAM_OUT, string_sink, &context);
  if (error) {
    goto cleanup;
  }

  output[output_length] = '\0';
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

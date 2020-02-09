#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Shows the output of the given command using `reproc_drain`.
int main(int argc, const char *argv[])
{
  (void) argc;

  reproc_t *process = NULL;
  char *output = NULL;
  int r = REPROC_ENOMEM;

  process = reproc_new();
  if (process == NULL) {
    goto finish;
  }

  r = reproc_start(process, argv + 1, (reproc_options){ 0 });
  if (r < 0) {
    goto finish;
  }

  r = reproc_close(process, REPROC_STREAM_IN);
  if (r < 0) {
    goto finish;
  }

  // `reproc_drain` reads from a child process and passes the output to the
  // given sinks. A sink consists of a function pointer and a context pointer
  // which is always passed to the function. reproc provides several built-in
  // sinks such as `reproc_sink_string` which stores all provided output in the
  // given string. Passing the same sink to both output streams makes sure the
  // output from both streams is combined into a single string.
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  if (r < 0) {
    goto finish;
  }

  if (output == NULL) {
    fprintf(stderr, "Failed to allocate memory for output\n");
    goto finish;
  }

  printf("%s", output);

  r = reproc_wait(process, REPROC_INFINITE);
  if (r < 0) {
    goto finish;
  }

finish:
  // Memory allocated by `reproc_sink_string` must be freed with `reproc_free`.
  reproc_free(output);
  reproc_destroy(process);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

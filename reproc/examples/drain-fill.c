#include <stdlib.h>

#include <reproc/drain.h>
#include <reproc/fill.h>
#include <reproc/reproc.h>

// Shows the output of the given command using `reproc_drain`.
int main(int argc, const char **argv)
{
  (void) argc;

  reproc_t *process = NULL;
  char *input = NULL;
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

  const size_t inSize = 1048576; // 1M
  input = malloc(inSize * sizeof(char));
  for (size_t i = 0; i < inSize; i++) {
    // make a 0123456789:;<=>?@ABCDEFG... string for testing
    input[i] = ((char) (i % (size_t) ('z' - '0' + 1)) + '0');
  }

  // `reproc_fill` writes to a child process using input from the given
  // filler.  A filler consists of a function pointer and a context pointer
  // which is always passed to the function.  reproc provides a built-in
  // filler `reproc_filler_buffer` which writes to stdin from the given
  // input buffer pointer/size pair.  This allows writing to stdin progressively
  // rather than using reproc_options which can overflow the input pipe.
  reproc_filler_buffer_ctx fillerCtx = { (uint8_t *) input, inSize, 0 };
  reproc_filler filler = reproc_filler_buffer(&fillerCtx);
  r = reproc_fill(process, filler);
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
  // By default, reproc only redirects stdout to a pipe and not stderr so we
  // pass `REPROC_SINK_NULL` as the sink for stderr here. We could also pass
  // `sink` but it wouldn't receive any data from stderr.
  r = reproc_drain(process, sink, REPROC_SINK_NULL);
  if (r < 0) {
    goto finish;
  }

  printf("%s", output);

  r = reproc_wait(process, REPROC_INFINITE);
  if (r < 0) {
    goto finish;
  }

finish:
  free(input);
  // Memory allocated by `reproc_sink_string` must be freed with `reproc_free`.
  reproc_free(output);
  reproc_destroy(process);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

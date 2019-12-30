#include <reproc/reproc.h>
#include <reproc/sink.h>

#undef NDEBUG
#include <assert.h>

static int io(const char *mode, const char *input, const char *expected)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[3] = { RESOURCE_DIRECTORY "/io", mode, NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  assert(r == 0);

  r = reproc_write(process, (uint8_t *) input, strlen(input));
  assert(r == 0);

  r = reproc_close(process, REPROC_STREAM_IN);
  assert(r == 0);

  char *output = NULL;
  r = reproc_drain(process, reproc_sink_string, &output);
  assert(r == 0);
  assert(output != NULL);

  assert(strcmp(output, expected) == 0);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  reproc_destroy(process);
  free(output);

  return 0;
}

#define MESSAGE "reproc stands for REdirected PROCess"

int main(void)
{
  io("stdout", MESSAGE, MESSAGE);
  io("stderr", MESSAGE, MESSAGE);
  io("both", MESSAGE, MESSAGE MESSAGE);

  return 0;
}

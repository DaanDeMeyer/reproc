#include <reproc/reproc.h>
#include <reproc/sink.h>

#undef NDEBUG
#include <assert.h>
#include <string.h>

static int io(const char *mode, const char *input, const char *expected)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[3] = { RESOURCE_DIRECTORY "/io", mode, NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  assert(r == 0);

  r = reproc_write(process, (uint8_t *) input, strlen(input), REPROC_INFINITE);
  assert(r == 0);

  r = reproc_close(process, REPROC_STREAM_IN);
  assert(r == 0);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink, REPROC_INFINITE);
  assert(r == 0);
  assert(output != NULL);

  assert(strcmp(output, expected) == 0);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  reproc_destroy(process);
  reproc_free(output);

  return 0;
}

static int timeout(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[3] = { RESOURCE_DIRECTORY "/io", "stdout", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  assert(r == 0);

  uint8_t buffer = 0;
  r = reproc_read(process, NULL, &buffer, sizeof(buffer), 200);
  assert(r == REPROC_ETIMEDOUT);

  r = reproc_close(process, REPROC_STREAM_IN);
  assert(r == 0);

  r = reproc_read(process, NULL, &buffer, sizeof(buffer), REPROC_INFINITE);
  assert(r == REPROC_EPIPE);

  reproc_destroy(process);

  return 0;
}

#define MESSAGE "reproc stands for REdirected PROCess"

int main(void)
{
  io("stdout", MESSAGE, MESSAGE);
  io("stderr", MESSAGE, MESSAGE);
  io("both", MESSAGE, MESSAGE MESSAGE);

  timeout();

  return 0;
}

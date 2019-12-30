#include <reproc/reproc.h>
#include <reproc/sink.h>

#undef NDEBUG
#include <assert.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[2] = { RESOURCE_DIRECTORY "/overflow", NULL };

  r = reproc_start(process, argv, (reproc_options) { 0 });
  assert(r >= 0);

  char *output = NULL;
  r = reproc_drain(process, reproc_sink_string, &output);
  assert(r >= 0);
  assert(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  reproc_destroy(process);
  free(output);

  return 0;
}

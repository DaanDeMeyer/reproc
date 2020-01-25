#include "assert.h"

#include <reproc/reproc.h>
#include <reproc/sink.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/overflow", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  assert(r >= 0);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  assert(r >= 0);
  assert(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  reproc_destroy(process);
  reproc_free(output);
}

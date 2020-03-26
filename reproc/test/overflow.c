#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/overflow", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  ASSERT_OK(r);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT_OK(r);
  ASSERT(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT_OK(r);

  reproc_destroy(process);
  reproc_free(output);
}

#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/argv", "\"argument 1\"",
                         "\"argument 2\"", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  ASSERT_OK(r);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT_OK(r);
  ASSERT(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT_OK(r);

  const char *current = output;

  for (size_t i = 0; i < 3; i++) {
    size_t size = strlen(argv[i]);

    ASSERT_GE_SIZE(strlen(current), size);
    ASSERT_EQ_MEM(current, argv[i], size);

    current += size;
  }

  ASSERT_EQ_SIZE(strlen(current), (size_t) 0);

  reproc_destroy(process);
  reproc_free(output);
}

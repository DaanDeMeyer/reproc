#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

#include <string.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/argv", "\"argument 1\"",
                         "\"argument 2\"", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  ASSERT(r >= 0);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT(r == 0);
  ASSERT(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT(r == 0);

  const char *current = output;

  for (size_t i = 0; i < 3; i++) {
    size_t size = strlen(argv[i]);

    ASSERT(strlen(current) >= size);
    ASSERT(memcmp(current, argv[i], size) == 0);

    current += size;
  }

  ASSERT(*current == '\0');

  reproc_destroy(process);
  reproc_free(output);
}

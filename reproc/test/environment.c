#include "assert.h"

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <string.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/environment", NULL };
  const char *envp[] = { "IP=127.0.0.1", "PORT=8080", NULL };

  r = reproc_start(process, argv, (reproc_options){ .environment = envp });
  ASSERT(r >= 0);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT(r == 0);
  ASSERT(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT(r == 0);

  const char *current = output;

  for (size_t i = 0; i < 2; i++) {
    size_t size = strlen(envp[i]);

    ASSERT(strlen(current) >= size);
    ASSERT(memcmp(current, envp[i], size) == 0);

    current += size;
  }

  ASSERT(*current == '\0');

  reproc_destroy(process);
  reproc_free(output);
}

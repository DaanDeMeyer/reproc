#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

#include <string.h>

static void replace(char *string, char old, char new)
{
  for (size_t i = 0; i < strlen(string); i++) {
    string[i] = (char) (string[i] == old ? new : string[i]);
  }
}

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/working-directory", NULL };

  r = reproc_start(process, argv,
                   (reproc_options){ .working_directory = RESOURCE_DIRECTORY });
  ASSERT(r >= 0);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT(r == 0);

  replace(output, '\\', '/');
  ASSERT(strcmp(output, RESOURCE_DIRECTORY) == 0);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT(r == 0);

  reproc_destroy(process);
  reproc_free(output);
}

#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

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
  ASSERT_OK(r);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT_OK(r);

  replace(output, '\\', '/');
  ASSERT_EQ_STR(output, RESOURCE_DIRECTORY);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT_OK(r);

  reproc_destroy(process);
  reproc_free(output);
}

#include <reproc/run.h>

#include "assert.h"

int main(void)
{
  const char *argv[] = { RESOURCE_DIRECTORY "/argv", "\"argument 1\"",
                         "\"argument 2\"", NULL };
  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  int r = -1;
  const char *current = NULL;
  size_t i = 0;
  reproc_options options = { 0 };

  r = reproc_run_ex(argv, options, sink, sink);
  ASSERT_OK(r);
  ASSERT(output != NULL);

  current = output;

  for (i = 0; i < 3; i++) {
    size_t size = strlen(argv[i]);

    ASSERT_GE_SIZE(strlen(current), size);
    ASSERT_EQ_MEM(current, argv[i], size);

    current += size;
    current += *current == '\r';
    current += *current == '\n';
  }

  ASSERT_EQ_SIZE(strlen(current), (size_t) 0);

  reproc_free(output);
}

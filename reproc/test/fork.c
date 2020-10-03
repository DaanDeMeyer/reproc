#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <reproc/run.h>

#include "assert.h"

static const char *MESSAGE = "reproc stands for REdirected PROCess!";

int main(void)
{
  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  int r = -1;

  r = reproc_run_ex(NULL, (reproc_options){ .fork = true }, sink, sink);

  if (r == 0) {
    printf("%s", MESSAGE);
    fclose(stdout); // `_exit` doesn't flush stdout.
    _exit(r < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  ASSERT_OK(r);
  ASSERT(output != NULL);
  ASSERT_EQ_STR(output, MESSAGE);

  reproc_free(output);
}

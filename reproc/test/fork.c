#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  r = reproc_start(process, NULL, (reproc_options){ .fork = true });
  ASSERT_OK(r);

  static const char *message = "reproc stands for REdirected PROCess!";

  if (r == 0) {
    r = (int) write(STDOUT_FILENO, message, strlen(message));
    _exit(r < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT_OK(r);
  ASSERT(output != NULL);

  ASSERT_EQ_STR(output, message);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT_OK(r);

  reproc_destroy(process);
  reproc_free(output);
}

#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  r = reproc_start(process, NULL, (reproc_options){ .fork = true });
  ASSERT_OK(r);

  int child_pid = reproc_pid(process);

  if (r == 0) {
    printf("%d", getpid());
    fclose(stdout); // `_exit` doesn't flush stdout.
    _exit(r < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink);
  ASSERT_OK(r);
  ASSERT(output != NULL);

  ASSERT(atoi(output) == child_pid);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT_OK(r);

  reproc_destroy(process);
  reproc_free(output);
}

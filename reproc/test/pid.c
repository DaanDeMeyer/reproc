#include "assert.h"

#include <reproc/drain.h>
#include <reproc/reproc.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
  const char *argv[] = { RESOURCE_DIRECTORY "/pid", NULL };
  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  int child_pid = reproc_pid(process);
  ASSERT(REPROC_EINVAL == child_pid);

  r = reproc_start(process, argv, (reproc_options){ 0 });
  ASSERT_OK(r);

  r = reproc_drain(process, sink, sink);
  ASSERT_OK(r);
  ASSERT(output != NULL);

  child_pid = reproc_pid(process);
  ASSERT(strtol(output, NULL, 10) == child_pid);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT_OK(r);

  child_pid = reproc_pid(process);
  ASSERT(strtol(output, NULL, 10) == child_pid);

  reproc_destroy(process);
  reproc_free(output);
}

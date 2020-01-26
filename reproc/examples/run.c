#include <reproc/run.h>

#include <stdio.h>
#include <stdlib.h>

// Start a process from the arguments given on the command line. Inherit the
// parent's standard streams and allow the process to run for maximum 5 seconds
// before terminating it. Since we inherit the standard streams of the parent,
// we pass `REPROC_SINK_NULL` twice as the sinks since there won't be any output
// to drain.
int main(int argc, const char *argv[])
{
  (void) argc;

  int r = reproc_run(argv + 1,
                     (reproc_options){ .redirect.parent = true,
                                       .deadline = 5000 },
                     REPROC_SINK_NULL, REPROC_SINK_NULL);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

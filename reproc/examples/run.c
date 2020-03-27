#include <reproc/run.h>

#include <stdlib.h>

// Start a process from the arguments given on the command line. Inherit the
// parent's standard streams and allow the process to run for maximum 5 seconds
// before terminating it.
int main(int argc, const char *argv[])
{
  (void) argc;

  int r = reproc_run(argv + 1, (reproc_options){ .deadline = 5000 });

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

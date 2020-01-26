#include <reproc/reproc.h>

#include <stdio.h>
#include <stdlib.h>

// Forwards the provided arguments to `reproc_start` which starts a child
// process that inherits the standard streams of the parent process. It will
// read its input from the stdin of this process and write its output to the
// stdout/stderr of this process.
int main(int argc, const char *argv[])
{
  if (argc <= 1) {
    fprintf(stderr,
            "No arguments provided. Example usage: ./inherit cmake --help");
    return 1;
  }

  reproc_t *process = NULL;
  int r = REPROC_ENOMEM;

  process = reproc_new();
  if (process == NULL) {
    goto finish;
  }

  r = reproc_start(process, argv + 1,
                   (reproc_options){ .redirect.parent = true });
  if (r < 0) {
    goto finish;
  }

  r = reproc_wait(process, REPROC_INFINITE);
  if (r < 0) {
    goto finish;
  }

finish:
  reproc_destroy(process);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

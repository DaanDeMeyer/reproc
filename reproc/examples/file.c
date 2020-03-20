#include <reproc/run.h>

#include <stdio.h>
#include <stdlib.h>

// Redirects the output of the given command to the reproc.out file.
int main(int argc, const char *argv[])
{
  (void) argc;

  FILE *file = fopen("reproc.out", "w");
  if (file == NULL) {
    return EXIT_FAILURE;
  }

  int r = reproc_run(argv + 1, (reproc_options){ .redirect.file = file });

  fclose(file);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

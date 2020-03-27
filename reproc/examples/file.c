#include <reproc/run.h>

#include <stdlib.h>

// Redirects the output of the given command to the reproc.out file.
int main(int argc, const char *argv[])
{
  (void) argc;

  FILE *file = fopen("reproc.out", "w");
  if (file == NULL) {
    fprintf(stderr, "Failed to open reproc.out for writing\n");
    return EXIT_FAILURE;
  }

  int r = reproc_run(argv + 1, (reproc_options){ .redirect.file = file });

  fclose(file);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#include <reproc/run.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[])
{
  (void) argc;

  FILE *file = fopen("reproc.out", "w");
  if (file == NULL) {
    return EXIT_FAILURE;
  }

  int r = reproc_run(argv + 1,
                     (reproc_options){
                         .redirect = { .stdio = { .out = REPROC_REDIRECT_FILE,
                                                  .err = REPROC_REDIRECT_FILE },
                                       .file = { .out = file, .err = file } } },
                     REPROC_SINK_NULL, REPROC_SINK_NULL);

  fclose(file);

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

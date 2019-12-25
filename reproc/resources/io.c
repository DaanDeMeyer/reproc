#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
  (void) argc;

  const char *mode = argv[1];
  char input[8096];

  if (fgets(input, sizeof(input), stdin) == NULL) {
    return 1;
  }

  if (strcmp(mode, "stdout") == 0 || strcmp(mode, "both") == 0) {
    fprintf(stdout, "%s", input);
  }

  if (strcmp(mode, "stderr") == 0 || strcmp(mode, "both") == 0) {
    fprintf(stderr, "%s", input);
  }

  return 0;
}

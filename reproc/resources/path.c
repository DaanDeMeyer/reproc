#include <stdio.h>

int main(int argc, const char **argv)
{
  (void) argc;

  printf("%s", argv[0]);

  fclose(stdout);

  return 0;
}

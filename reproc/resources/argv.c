#include <stdio.h>

int main(int argc, const char **argv)
{
  int i;
  for (i = 0; i < argc; i++) {
    printf("%s\n", argv[i]);
  }

  return 0;
}

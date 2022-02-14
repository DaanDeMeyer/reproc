#include <stdio.h>

int main(int argc, const char **argv)
{
  int i = 0;
  for (i = 0; i < argc; i++) {
    printf("%s\n", argv[i]);
  }

  return 0;
}

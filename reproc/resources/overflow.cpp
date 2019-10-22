#include <cstdio>
#include <cstdlib>

int main()
{
  char buffer[8192]; // NOLINT

  for (int i = 0; i < 200; i++) {
    FILE *stream = rand() % 2 == 0 ? stdout : stderr; // NOLINT
    fprintf(stream, "%s", buffer); // NOLINT
  }

  return 0;
}

#include <reproc/run.h>

#include "assert.h"

int main(void)
{
  const char *argv[] = { RESOURCE_DIRECTORY "/path", NULL };
  int r = -1;
  FILE *file = NULL;
  size_t size = 0;
  char *string = NULL;

  {
    reproc_options options = { 0 };
    options.redirect.path = "path.txt";
    r = reproc_run(argv, options);
  }
  ASSERT_OK(r);

  file = fopen("path.txt", "rb");
  ASSERT(file != NULL);

  r = fseek(file, 0, SEEK_END);
  ASSERT_OK(r);

  r = (int) ftell(file);
  ASSERT_OK(r);

  size = (size_t) r;
  string = malloc(size + 1);
  ASSERT(string != NULL);

  rewind(file);
  r = (int) fread(string, sizeof(char), size, file);
  ASSERT_EQ_INT(r, (int) size);

  string[r] = '\0';

  r = fclose(file);
  ASSERT_OK(r);

  r = remove("path.txt");
  ASSERT_OK(r);

  ASSERT_EQ_STR(string, argv[0]);

  free(string);
}

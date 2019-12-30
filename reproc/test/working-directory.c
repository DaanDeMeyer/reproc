#include <reproc/reproc.h>
#include <reproc/sink.h>

#undef NDEBUG
#include <assert.h>

static void replace(char *string, char old, char new)
{
  for (size_t i = 0; i < strlen(string); i++) {
    string[i] = (char) (string[i] == old ? new : string[i]);
  }
}

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[2] = { RESOURCE_DIRECTORY "/working-directory", NULL };

  r = reproc_start(process, argv,
                   (reproc_options){ .working_directory = RESOURCE_DIRECTORY });
  assert(r == 0);

  char *output = NULL;
  r = reproc_drain(process, reproc_sink_string, &output);
  assert(r == 0);

  replace(output, '\\', '/');
  assert(strcmp(output, RESOURCE_DIRECTORY) == 0);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  reproc_destroy(process);
  free(output);

  return 0;
}

#include <reproc/reproc.h>
#include <reproc/sink.h>

#undef NDEBUG
#include <assert.h>

int main(void)
{
  char *output = NULL;
  int r = REPROC_ENOMEM;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[4] = { RESOURCE_DIRECTORY "/argv", "\"argument 1\"",
                          "\"argument 2\"", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  assert(r == 0);

  r = reproc_drain(process, reproc_sink_string, &output);
  assert(r == 0);
  assert(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  const char *current = output;

  for (size_t i = 0; i < 3; i++) {
    size_t size = strlen(argv[i]);

    assert(strlen(current) >= size);
    assert(memcmp(current, argv[i], size) == 0);

    current += size;
  }

  assert(*current == '\0');

  reproc_destroy(process);
  free(output);

  return 0;
}

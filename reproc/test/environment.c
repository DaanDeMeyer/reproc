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

  const char *argv[2] = { RESOURCE_DIRECTORY "/environment", NULL };
  const char *envp[3] = { "IP=127.0.0.1", "PORT=8080", NULL };

  r = reproc_start(process, argv, (reproc_options) { .environment = envp });
  assert(r == 0);

  r = reproc_drain(process, reproc_sink_string, &output);
  assert(r == 0);
  assert(output != NULL);

  r = reproc_wait(process, REPROC_INFINITE);
  assert(r == 0);

  const char *current = output;

  for (size_t i = 0; i < 2; i++) {
    size_t size = strlen(envp[i]);

    assert(strlen(current) >= size);
    assert(memcmp(current, envp[i], size) == 0);

    current += size;
  }

  assert(*current == '\0');

  reproc_destroy(process);
  free(output);

  return 0;
}

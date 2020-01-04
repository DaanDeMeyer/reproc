#include <reproc/reproc.h>
#include <reproc/sink.h>

#undef NDEBUG
#include <assert.h>
#include <string.h>

int main(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[2] = { RESOURCE_DIRECTORY "/environment", NULL };
  const char *envp[3] = { "IP=127.0.0.1", "PORT=8080", NULL };

  r = reproc_start(process, argv, (reproc_options) { .environment = envp });
  assert(r == 0);

  char *output = NULL;
  reproc_sink sink = reproc_sink_string(&output);
  r = reproc_drain(process, sink, sink, REPROC_INFINITE);
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
  reproc_free(output);
}

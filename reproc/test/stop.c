#include "assert.h"

#include <reproc/reproc.h>

static void stop(REPROC_STOP action, int status)
{
  int r = -1;

  reproc_t *process = reproc_new();
  assert(process);

  const char *argv[2] = { RESOURCE_DIRECTORY "/stop", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  assert(r >= 0);

  r = reproc_wait(process, 50);
  assert(r == REPROC_ETIMEDOUT);

  r = reproc_stop(process, (reproc_stop_actions){ .first = { action, 50 } });
  assert(r == status);

  reproc_destroy(process);
}

int main(void)
{
  stop(REPROC_STOP_TERMINATE, REPROC_SIGTERM);
  stop(REPROC_STOP_KILL, REPROC_SIGKILL);
}

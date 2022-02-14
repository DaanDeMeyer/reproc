#include <reproc/run.h>

#include "assert.h"

int main(void)
{
  const char *argv[] = { RESOURCE_DIRECTORY "/deadline", NULL };
  int r;
  {
    reproc_options options = { 0 };
    options.deadline = 100;
    r = reproc_run(argv, options);
  }
  ASSERT(r == REPROC_SIGTERM);
}

#include <process.h>
#include <stdio.h>
#include <stdbool.h>

int main(void)
{
  Process *process;
  PROCESS_LIB_ERROR error;

  error = process_init(&process);
  if (error) { return 1; }

  int argc = 2;
  const char *argv[3] = {"cmake", "--help", 0};

  error = process_start(process, argc, argv, 0);
  if (error) { return 1; }

  char buffer[1024];
  uint32_t buffer_size = 1024;
  uint32_t actual;

  while (true) {
    error = process_read(process, buffer, buffer_size - 1, &actual);
    if (error) { break; }

    buffer[actual] = '\0';
    fprintf(stdout, "%s", buffer);
  }

  fflush(stdout);

  return 0;
}

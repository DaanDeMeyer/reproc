#include <process.h>
#include <stdio.h>
#include <stdbool.h>

int main(void)
{
  process_type *process;
  PROCESS_LIB_ERROR error;

  error = process_init(&process);
  if (error) { return 1; }

  int argc = 2;
  const char *argv[3] = {"cmake", "--help", 0};

  error = process_start(process, argv, argc, 0);
  if (error) { return 1; }

  char buffer[1024];
  unsigned int buffer_size = 1024;
  unsigned int actual;

  while (true) {
    error = process_read(process, buffer, buffer_size - 1, &actual);
    if (error) { break; }

    buffer[actual] = '\0';
    fprintf(stdout, "%s", buffer);
  }

  if (error != PROCESS_LIB_STREAM_CLOSED) { return 1; }

  fflush(stdout);

  error = process_wait(process, 50);
  if (error) { return 1; }

  int exit_status;
  error = process_exit_status(process, &exit_status);
  if (error) { return 1; }

  error = process_free(&process);
  if (error) { return 1; }

  return exit_status;
}

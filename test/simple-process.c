#include <malloc.h>
#include <process.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  char *args[3] = {"echo", "working", 0};
  char buffer[1000];
  uint32_t actual = 0;

  Process *process = process_alloc();
  PROCESS_LIB_ERROR error = process_init(process);
  error = error ? error : process_start(process, 2, args);
  error = error ? error : process_read(process, buffer, 10, &actual);
  buffer[actual] = '\0';
  printf("%s\n", buffer);

  error = error ? error : process_terminate(process, 1000);
  error = error ? error : process_free(process);

  return error;
}
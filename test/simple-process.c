#include <malloc.h>
#include <process.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char *argv[])
{
  char *args[2] = {"build/echo", 0};
  char buffer[1000];
  uint32_t actual = 0;

  Process *process = process_alloc();
  if (process == NULL) {
    fprintf(stderr, "%s\n", "Error allocating memory");
  }

  PROCESS_LIB_ERROR error = process_init(process);
  if (error) { 
    fprintf(stderr, "%s\n", process_error_to_string(error));
  }

  error = process_start(process, 1, args);
  if (error) {
    
  }
  error = error ? error : process_read(process, buffer, 10, &actual);
  buffer[actual] = '\0';
  printf("%s\n", buffer);

  error = error ? error : process_terminate(process, 1000);
  error = error ? error : process_free(process);

  return error;
}
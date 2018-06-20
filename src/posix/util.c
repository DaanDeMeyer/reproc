#include "util.h"

#include <errno.h>
#include <unistd.h>

PROCESS_ERROR pipe_init(int *read, int *write)
{
  int pipefd[2];
  int result = pipe(pipefd);

  if (result == -1) { return system_error_to_process_error(errno); }

  // Assign file desccriptors if create pipe was succesful
  *read = pipefd[0];
  *write = pipefd[1];

  return PROCESS_SUCCESS;
}

PROCESS_ERROR system_error_to_process_error(int system_error)
{
  switch (system_error) {
  case 0:
    return PROCESS_SUCCESS;
  default:
    return PROCESS_UNKNOWN_ERROR;
  }
}
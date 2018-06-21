#include "util.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>

PROCESS_LIB_ERROR pipe_init(int *read, int *write)
{
  int pipefd[2];

  errno = 0;

  int result = pipe(pipefd);

  if (result == -1) { return system_error_to_process_error(errno); }

  // Assign file desccriptors if create pipe was succesful
  *read = pipefd[0];
  *write = pipefd[1];

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_write(int pipe, const void *buffer, uint32_t to_write,
                             uint32_t *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  errno = 0;

  *actual = write(pipe, buffer, to_write);

  // write returns -1 on error which is the max unsigned value
  if (*actual == UINT32_MAX) { return system_error_to_process_error(errno); }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR pipe_read(int pipe, void *buffer, uint32_t to_read,
                            uint32_t *actual)
{
  assert(pipe);
  assert(buffer);
  assert(actual);

  errno = 0;

  *actual = read(pipe, buffer, to_read);

  if (*actual == UINT32_MAX) { return system_error_to_process_error(errno); }

  return PROCESS_LIB_SUCCESS;
}

PROCESS_LIB_ERROR system_error_to_process_error(int system_error)
{
  switch (system_error) {
  case 0:
    return PROCESS_LIB_SUCCESS;
  default:
    return PROCESS_LIB_UNKNOWN_ERROR;
  }
}

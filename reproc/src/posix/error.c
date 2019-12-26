#include <reproc/reproc.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

const int REPROC_ERROR_STREAM_CLOSED = -EPIPE;
const int REPROC_ERROR_WAIT_TIMEOUT = -EAGAIN;

const char *reproc_error_string(int error)
{
  return strerror(abs(error));
}

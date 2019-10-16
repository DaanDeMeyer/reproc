#include <reproc/error.h>

#include <errno.h>
#include <string.h>

unsigned int reproc_error_system(void)
{
  // Only positive numbers are valid `errno` values so casting to unsigned int
  // is safe.
  return (unsigned int) errno;
}

const char *reproc_error_string(REPROC_ERROR error)
{
  switch (error) {
    case REPROC_SUCCESS:
      return "success";
    case REPROC_ERROR_WAIT_TIMEOUT:
      return "wait timeout";
    case REPROC_ERROR_STREAM_CLOSED:
      return "stream closed";
    case REPROC_ERROR_PARTIAL_WRITE:
      return "partial write";
    case REPROC_ERROR_SYSTEM:
      // `reproc_error_system` returns `errno` which is always in `int` range.
      return strerror((int) reproc_error_system());
  }

  return "unknown error";
}

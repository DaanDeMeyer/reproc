#include <reproc/error.h>

#include <windows.h>

unsigned int reproc_system_error(void)
{
  return GetLastError();
}

const char *reproc_strerror(REPROC_ERROR error)
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
      return "system error";
  }

  return "unknown error";
}

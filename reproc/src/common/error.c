#include <reproc/reproc.h>

const char *reproc_error_to_string(REPROC_ERROR error)
{
  switch (error) {
  case REPROC_SUCCESS: return "success";
  case REPROC_WAIT_TIMEOUT: return "wait timeout";
  case REPROC_STREAM_CLOSED: return "stream closed";
  case REPROC_PARTIAL_WRITE: return "partial write";
  case REPROC_NOT_ENOUGH_MEMORY: return "memory error";
  case REPROC_PIPE_LIMIT_REACHED: return "pipe limit reached";
  case REPROC_INTERRUPTED: return "interrupted";
  case REPROC_PROCESS_LIMIT_REACHED: return "process limit reached";
  case REPROC_INVALID_UNICODE: return "invalid unicode";
  case REPROC_PERMISSION_DENIED: return "permission denied";
  case REPROC_SYMLINK_LOOP: return "symlink loop";
  case REPROC_FILE_NOT_FOUND: return "file not found";
  case REPROC_NAME_TOO_LONG: return "name too long";
  case REPROC_ARGS_TOO_LONG: return "args too long";
  case REPROC_NOT_EXECUTABLE: return "not executable";
  case REPROC_UNKNOWN_ERROR: return "unknown error";
  }

  return "unknown error";
}

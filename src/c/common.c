#include "reproc/reproc.h"

const char *reproc_error_to_string(REPROC_ERROR error)
{
  switch (error) {
  case REPROC_SUCCESS: return "reproc => success";
  case REPROC_UNKNOWN_ERROR: return "reproc => unknown error";
  case REPROC_WAIT_TIMEOUT: return "reproc => wait timeout";
  case REPROC_STREAM_CLOSED: return "reproc => stream closed";
  case REPROC_STILL_RUNNING: return "reproc: still running";
  case REPROC_MEMORY_ERROR: return "reproc => memory error";
  case REPROC_PIPE_LIMIT_REACHED: return "reproc => pipe limit reached";
  case REPROC_INTERRUPTED: return "reproc => interrupted";
  case REPROC_PROCESS_LIMIT_REACHED:
    return "reproc => process limit reached";
  case REPROC_INVALID_UNICODE: return "reproc => invalid unicode";
  case REPROC_PERMISSION_DENIED: return "reproc => permission denied";
  case REPROC_SYMLINK_LOOP: return "reproc => symlink loop";
  case REPROC_FILE_NOT_FOUND: return "reproc => file not found";
  case REPROC_NAME_TOO_LONG: return "reproc => name too long";
  case REPROC_PARTIAL_WRITE: return "reproc => partial write";
  }

  return "reproc => error has no string";
}

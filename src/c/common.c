#include "process-lib/process.h"

const char *process_error_to_string(PROCESS_LIB_ERROR error)
{
  switch (error) {
  case PROCESS_LIB_SUCCESS: return "process-lib => success";
  case PROCESS_LIB_UNKNOWN_ERROR: return "process-lib => unknown error";
  case PROCESS_LIB_WAIT_TIMEOUT: return "process-lib => wait timeout";
  case PROCESS_LIB_STREAM_CLOSED: return "process-lib => stream closed";
  case PROCESS_LIB_STILL_RUNNING: return "process-lib: still running";
  case PROCESS_LIB_MEMORY_ERROR: return "process-lib => memory error";
  case PROCESS_LIB_PIPE_LIMIT_REACHED: return "process-lib => pipe limit reached";
  case PROCESS_LIB_INTERRUPTED: return "process-lib => interrupted";
  case PROCESS_LIB_PROCESS_LIMIT_REACHED:
    return "process-lib => process limit reached";
  case PROCESS_LIB_INVALID_UNICODE: return "process-lib => invalid unicode";
  case PROCESS_LIB_PERMISSION_DENIED: return "process-lib => permission denied";
  case PROCESS_LIB_SYMLINK_LOOP: return "process-lib => symlink loop";
  case PROCESS_LIB_FILE_NOT_FOUND: return "process-lib => file not found";
  case PROCESS_LIB_NAME_TOO_LONG: return "process-lib => name too long";
  case PROCESS_LIB_PARTIAL_WRITE: return "process-lib => partial write";
  }

  return "process-lib => error has no string";
}

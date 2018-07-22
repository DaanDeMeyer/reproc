#ifndef REPROC_ERROR_HPP
#define REPROC_ERROR_HPP

#include "export.hpp"

#include <string>

namespace reproc {

/*! \see REPROC_ERROR */
/* When editing make sure to change the corresponding enum in error.h as
well */
enum Error {
  SUCCESS,
  UNKNOWN_ERROR,
  WAIT_TIMEOUT,
  STREAM_CLOSED,
  STILL_RUNNING,
  MEMORY_ERROR,
  PIPE_LIMIT_REACHED,
  INTERRUPTED,
  PROCESS_LIMIT_REACHED,
  INVALID_UNICODE,
  PERMISSION_DENIED,
  SYMLINK_LOOP,
  FILE_NOT_FOUND,
  NAME_TOO_LONG,
  PARTIAL_WRITE,
};

/*! \see reproc_system_error */
REPROC_EXPORT unsigned int system_error();

/*! \see reproc_error_to_string. This function additionally adds the system
error to the error string when /p error is Reproc::UNKNOWN_ERROR. */
REPROC_EXPORT std::string error_to_string(Error error);

} // namespace reproc

#endif

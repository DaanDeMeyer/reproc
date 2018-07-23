#ifndef REPROC_ERROR_HPP
#define REPROC_ERROR_HPP

#include "export.hpp"

#include <string>

namespace reproc {

/*! \see REPROC_ERROR */
/* When editing make sure to change the corresponding enum in error.h as
well */
enum error {
  success,
  unknown_error,
  wait_timeout,
  stream_closed,
  memory_error,
  pipe_limit_reached,
  interrupted,
  process_limit_reached,
  invalid_unicode,
  permission_denied,
  symlink_loop,
  file_not_found,
  name_too_long,
  partial_write,
};

/*! \see reproc_system_error */
REPROC_EXPORT unsigned int system_error();

/*! \see reproc_error_to_string. This function additionally adds the system
error to the error string when /p error is Reproc::UNKNOWN_ERROR. */
REPROC_EXPORT std::string error_to_string(reproc::error error);

} // namespace reproc

#endif

#ifndef REPROC_ERROR_HPP
#define REPROC_ERROR_HPP

namespace reproc {

/*! \see REPROC_ERROR */
/* When editing make sure to change the corresponding enum in reproc.h as
well */
enum error {
  success,
  unknown_error,
  wait_timeout,
  stream_closed,
  still_running,
  memory_error,
  pipe_limit_reached,
  interrupted,
  process_limit_reached,
  invalid_unicode,
  permission_denied,
  symlink_loop,
  file_not_found,
  name_too_long,
  partial_write
};

} // namespace reproc

#endif

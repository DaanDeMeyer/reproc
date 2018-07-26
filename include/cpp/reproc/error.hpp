#ifndef REPROC_ERROR_HPP
#define REPROC_ERROR_HPP

#include "export.hpp"

#include <system_error>

namespace reproc
{

/*! \see REPROC_ERROR */
/* When editing make sure to change the corresponding enum in error.h as
well */
enum class error {
  // reproc errors
  wait_timeout = 1,
  stream_closed,
  partial_write,
  // system errors
  not_enough_memory,
  pipe_limit_reached,
  interrupted,
  process_limit_reached,
  invalid_unicode,
  permission_denied,
  symlink_loop,
  file_not_found,
  name_too_long,
  unknown_error
};

REPROC_EXPORT const std::error_category &error_category() noexcept;

REPROC_EXPORT std::error_condition
make_error_condition(reproc::error error) noexcept;

} // namespace reproc

namespace std
{

template <> struct is_error_condition_enum<reproc::error> : true_type {
};

} // namespace std

#endif

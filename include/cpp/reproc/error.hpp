/*! \file error.hpp */

#ifndef REPROC_ERROR_HPP
#define REPROC_ERROR_HPP

#include "export.hpp"

#include <system_error>

/*! \namespace reproc */
namespace reproc
{

/*! \see REPROC_ERROR */
/* When editing make sure to change the corresponding enum in error.h as well */
enum class errc {
  // reproc errors

  /*! #REPROC_WAIT_TIMEOUT */
  wait_timeout = 1,
  /*! #REPROC_STREAM_CLOSED */
  stream_closed,
  /*! #REPROC_PARTIAL_WRITE */
  partial_write,

  // system errors

  /*! #REPROC_NOT_ENOUGH_MEMORY */
  not_enough_memory,
  /*! #REPROC_PIPE_LIMIT_REACHED */
  pipe_limit_reached,
  /*! #REPROC_INTERRUPTED */
  interrupted,
  /*! #REPROC_PROCESS_LIMIT_REACHED */
  process_limit_reached,
  /*! #REPROC_INVALID_UNICODE */
  invalid_unicode,
  /*! #REPROC_PERMISSION_DENIED */
  permission_denied,
  /*! #REPROC_SYMLINK_LOOP */
  symlink_loop,
  /*! #REPROC_FILE_NOT_FOUND */
  file_not_found,
  /*! #REPROC_WAIT_TIMEOUT */
  name_too_long
};

/*! \private */
REPROC_EXPORT const std::error_category &error_category() noexcept;

/*! \private */
REPROC_EXPORT std::error_condition
make_error_condition(reproc::errc error) noexcept;

} // namespace reproc

namespace std
{

template <> struct is_error_condition_enum<reproc::errc> : true_type {
};

} // namespace std

#endif

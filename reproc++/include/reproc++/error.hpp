#ifndef REPROC_ERROR_HPP
#define REPROC_ERROR_HPP

#include <reproc++/export.hpp>

#include <system_error>

namespace reproc
{

/*! `REPROC_ERROR` */
// When changing this enum make sure to change the corresponding enum in error.h
// as well.
enum class errc {
  // reproc errors

  /*! `REPROC_WAIT_TIMEOUT` */
  wait_timeout = 1,
  /*! `REPROC_STREAM_CLOSED` */
  stream_closed = 2,
  /*! `REPROC_PARTIAL_WRITE` */
  partial_write = 3,

  // system errors

  /*! `REPROC_NOT_ENOUGH_MEMORY` */
  not_enough_memory = 4,
  /*! `REPROC_PIPE_LIMIT_REACHED` */
  pipe_limit_reached = 5,
  /*! `REPROC_INTERRUPTED` */
  interrupted = 6,
  /*! `REPROC_PROCESS_LIMIT_REACHED` */
  process_limit_reached = 7,
  /*! `REPROC_INVALID_UNICODE` */
  invalid_unicode = 8,
  /*! `REPROC_PERMISSION_DENIED` */
  permission_denied = 9,
  /*! `REPROC_SYMLINK_LOOP` */
  symlink_loop = 10,
  /*! `REPROC_FILE_NOT_FOUND` */
  file_not_found = 11,
  /*! `REPROC_NAME_TOO_LONG` */
  name_too_long = 12,
  /*! `REPROC_ARGS_TOO_LONG` */
  args_too_long = 13,
  /*! `REPROC_NOT_EXECUTABLE` */
  not_executable = 14
};

REPROCXX_EXPORT const std::error_category &error_category() noexcept;

REPROCXX_EXPORT std::error_condition
make_error_condition(reproc::errc error) noexcept;

} // namespace reproc

namespace std
{

template <> struct is_error_condition_enum<reproc::errc> : true_type {
};

} // namespace std

#endif

#pragma once

#include <reproc++/export.hpp>

#include <system_error>

namespace reproc {

/*! `REPROC_ERROR` */
// When changing this enum make sure to change the corresponding enum in error.h
// as well.
enum class error {
  /*! `REPROC_SUCCESS` */
  success,
  /*! `REPROC_ERROR_WAIT_TIMEOUT` */
  wait_timeout,
  /*! `REPROC_ERROR_STREAM_CLOSED` */
  stream_closed
};

REPROCXX_EXPORT const std::error_category &error_category() noexcept;

REPROCXX_EXPORT std::error_code make_error_code(error error) noexcept;

} // namespace reproc

namespace std {

template <>
struct is_error_code_enum<reproc::error> : true_type {};

} // namespace std

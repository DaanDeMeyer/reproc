#include <reproc++/error.hpp>

#include <reproc/error.h>

#include <string>

namespace reproc {

class error_category_impl : public std::error_category {
public:
  const char *name() const noexcept override;

  std::string message(int condition) const noexcept override;

  bool equivalent(const std::error_code &code, int condition) const
      noexcept override;
};

const char *error_category_impl::name() const noexcept
{
  return "reproc";
}

std::string error_category_impl::message(int condition) const noexcept
{
  return reproc_strerror(static_cast<REPROC_ERROR>(condition));
}

bool error_category_impl::equivalent(const std::error_code &code,
                                     int condition) const noexcept
{
  switch (static_cast<error>(condition)) {
      // `REPROC_WAIT_TIMEOUT`, `REPROC_STREAM_CLOSED` and
      // `REPROC_PARTIAL_WRITE` are reproc-specific errors that do not
      // correspond to an operating system error. They are converted to
      // `std::error_code`'s of the reproc category with the same value in
      // `error_to_error_code` (reproc.cpp). The matching values in
      // `reproc::error` are defined with the same value and as a result,
      // checking error codes for equivalence against these values comes down to
      // checking if they belong to the reproc category and if their values are
      // the same.

    case error::wait_timeout:
    case error::stream_closed:
    case error::partial_write:
      return code.category() == *this && code.value() == condition;

      // The rest of the values from `REPROC_ERROR` are converted back to their
      // platform-specific equivalent error code of the system category (see
      // `error_to_error_code` in `reproc.cpp`). These already have equivalence
      // defined with the `std::errc` error condition values so for the
      // remaining `reproc::error` enum values, checking for equivalence with an
      // error code is the same as checking for equivalence with the
      // corresponding value from `std::errc`.

    case error::not_enough_memory:
      return code == std::errc::not_enough_memory;
    case error::pipe_limit_reached:
      return code == std::errc::too_many_files_open ||
             code == std::errc::too_many_files_open_in_system;
    case error::interrupted:
      return code == std::errc::interrupted;
    case error::process_limit_reached:
      return code == std::errc::resource_unavailable_try_again ||
             code == std::errc::too_many_files_open;
    case error::permission_denied:
      return code == std::errc::permission_denied ||
             code == std::errc::operation_not_permitted;
    case error::symlink_loop:
      return code == std::errc::too_many_symbolic_link_levels;
    case error::file_not_found:
      return code == std::errc::no_such_file_or_directory;
    case error::name_too_long:
      return code == std::errc::filename_too_long;
    case error::args_too_long:
      return code == std::errc::argument_list_too_long;
    case error::not_executable:
      return code == std::errc::executable_format_error;

      // `invalid_unicode` does not have an `std::errc` equivalent and is only
      // valid on Windows so on Windows we check if the error code is of the
      // system category and its value is the same as the value assigned to
      // `ERROR_NO_UNICODE_TRANSLATION`.

    case error::invalid_unicode:
#ifdef _WIN32
      // `ERROR_NO_UNICODE_TRANSLATION` == 1113 (Windows).
      return code.category() == std::system_category() && code.value() == 1113;
#else
      // `REPROC_INVALID_UNICODE` is Windows specific so it can't happen on
      // POSIX systems.
      return false;
#endif
  }

  return false;
}

const std::error_category &error_category() noexcept
{
  static error_category_impl instance;
  return instance;
}

std::error_condition make_error_condition(error error) noexcept
{
  return { static_cast<int>(error), error_category() };
}

} // namespace reproc

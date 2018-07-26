#include "reproc/error.hpp"

#include "reproc/error.h"

#include <string>

namespace reproc
{

class error_category_impl : public std::error_category
{
public:
  const char *name() const noexcept override { return "reproc"; }

  std::string message(int ev) const noexcept override
  {
    return reproc_error_to_string(static_cast<REPROC_ERROR>(ev));
  }

  bool equivalent(const std::error_code &error_code, int error_condition) const
      noexcept override
  {
    switch (static_cast<reproc::error>(error_condition)) {
    // reproc errors
    case reproc::error::wait_timeout:
      return error_code == reproc::error::wait_timeout;
    case reproc::error::stream_closed:
      return error_code == reproc::error::stream_closed;
    case reproc::error::partial_write:
      return error_code == reproc::error::partial_write;
    // system errors
    case reproc::error::not_enough_memory:
      return error_code == std::errc::not_enough_memory;
    case reproc::error::pipe_limit_reached:
      return error_code == std::errc::too_many_files_open ||
             error_code == std::errc::too_many_files_open_in_system;
    case reproc::error::interrupted:
      return error_code == std::errc::interrupted;
    case reproc::error::process_limit_reached:
      return error_code == std::errc::resource_unavailable_try_again;
    case reproc::error::invalid_unicode:
      // 1113 == ERROR_NO_UNICODE_TRANSLATION (Windows)
      return error_code == std::error_code(1113, std::system_category());
    case reproc::error::permission_denied:
      return error_code == std::errc::permission_denied;
    case reproc::error::symlink_loop:
      return error_code == std::errc::too_many_symbolic_link_levels;
    case reproc::error::file_not_found:
      return error_code == std::errc::no_such_file_or_directory;
    case reproc::error::name_too_long:
      return error_code == std::errc::filename_too_long;
    case reproc::error::unknown_error:
      return error_code.category() == std::system_category();
    default:
      return false;
    }
  }
};

const std::error_category &error_category() noexcept
{
  static reproc::error_category_impl instance;
  return instance;
}

std::error_condition make_error_condition(reproc::error error) noexcept
{
  return { static_cast<int>(error), reproc::error_category() };
}

} // namespace reproc

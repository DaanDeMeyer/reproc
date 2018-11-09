#include "reproc++/error.hpp"

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
    switch (static_cast<reproc::errc>(error_condition)) {
    // reproc errors are handled identically so we use switch fallthrough.
    case reproc::errc::wait_timeout:
    case reproc::errc::stream_closed:
    case reproc::errc::partial_write:
      // This check comes down to checking if the value from REPROC_ERROR
      // matches the value from the reproc::errc value it is checked against.
      // To avoid conflicting with other error codes we also require the error
      // code to be in the reproc error category. Look at
      // reproc_error_to_error_code in reproc.cpp to see how the reproc specific
      // error codes are constructed.
      return error_code ==
             std::error_code(error_condition, reproc::error_category());

    // The rest of the reproc errors are all system errors so we can just check
    // against the standard error conditions from std which already do all the
    // work for us.
    case reproc::errc::not_enough_memory:
      return error_code == std::errc::not_enough_memory;
    case reproc::errc::pipe_limit_reached:
      return error_code == std::errc::too_many_files_open ||
             error_code == std::errc::too_many_files_open_in_system;
    case reproc::errc::interrupted: return error_code == std::errc::interrupted;
    case reproc::errc::process_limit_reached:
      return error_code == std::errc::resource_unavailable_try_again ||
             error_code == std::errc::too_many_files_open;
    case reproc::errc::invalid_unicode:
#ifdef _WIN32
      // ERROR_NO_UNICODE_TRANSLATION == 1113 (Windows).
      return error_code == std::error_code(1113, std::system_category());
#else
      // REPROC_INVALID_UNICODE is Windows specific so it can't happen on POSIX
      // systems.
      return false;
#endif
    case reproc::errc::permission_denied:
      return error_code == std::errc::permission_denied ||
             error_code == std::errc::operation_not_permitted;
    case reproc::errc::symlink_loop:
      return error_code == std::errc::too_many_symbolic_link_levels;
    case reproc::errc::file_not_found:
      return error_code == std::errc::no_such_file_or_directory;
    case reproc::errc::name_too_long:
      return error_code == std::errc::filename_too_long;
    case reproc::errc::args_too_long:
      return error_code == std::errc::argument_list_too_long;
    case reproc::errc::not_executable:
      return error_code == std::errc::executable_format_error;
    }

    return false;
  }
};

const std::error_category &error_category() noexcept
{
  static reproc::error_category_impl instance;
  return instance;
}

std::error_condition make_error_condition(reproc::errc error) noexcept
{
  return { static_cast<int>(error), reproc::error_category() };
}

} // namespace reproc

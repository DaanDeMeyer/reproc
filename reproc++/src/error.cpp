#include <reproc++/error.hpp>

#include <reproc/error.h>

#include <string>

namespace reproc {

class error_category_impl : public std::error_category {
public:
  const char *name() const noexcept override;

  std::string message(int condition) const noexcept override;

  bool equivalent(int code, const std::error_condition &condition) const
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

bool error_category_impl::equivalent(
    int code,
    const std::error_condition &condition) const noexcept
{
  if (condition == default_error_condition(code)) {
    return true;
  }

  switch (static_cast<reproc::error>(code)) {
    // The rest of the reproc errors are all system errors so we can just check
    // against the standard error conditions from `std::error` which do all the
    // work for us.
    case reproc::error::not_enough_memory:
      return condition == std::errc::not_enough_memory;
    case reproc::error::pipe_limit_reached:
      return condition == std::errc::too_many_files_open ||
             condition == std::errc::too_many_files_open_in_system;
    case reproc::error::interrupted:
      return condition == std::errc::interrupted;
    case reproc::error::process_limit_reached:
      return condition == std::errc::resource_unavailable_try_again ||
             condition == std::errc::too_many_files_open;
    case reproc::error::permission_denied:
      return condition == std::errc::permission_denied ||
             condition == std::errc::operation_not_permitted;
    case reproc::error::symlink_loop:
      return condition == std::errc::too_many_symbolic_link_levels;
    case reproc::error::file_not_found:
      return condition == std::errc::no_such_file_or_directory;
    case reproc::error::name_too_long:
      return condition == std::errc::filename_too_long;
    case reproc::error::args_too_long:
      return condition == std::errc::argument_list_too_long;
    case reproc::error::not_executable:
      return condition == std::errc::executable_format_error;
    default:
      break;
  }

  return false;
}

const std::error_category &error_category() noexcept
{
  static reproc::error_category_impl instance;
  return instance;
}

std::error_code make_error_code(reproc::error error) noexcept
{
  return { static_cast<int>(error), reproc::error_category() };
}

} // namespace reproc

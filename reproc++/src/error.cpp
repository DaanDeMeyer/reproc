#include <reproc++/error.hpp>

#include <reproc/error.h>

#include <string>

namespace reproc {

class error_category_impl : public std::error_category {
public:
  const char *name() const noexcept override;

  std::string message(int code) const noexcept override;
};

const char *error_category_impl::name() const noexcept
{
  return "reproc";
}

std::string error_category_impl::message(int code) const noexcept
{
  return reproc_strerror(static_cast<REPROC_ERROR>(code));
}

const std::error_category &error_category() noexcept
{
  static error_category_impl instance;
  return instance;
}

std::error_code make_error_code(error error) noexcept
{
  return { static_cast<int>(error), error_category() };
}

} // namespace reproc

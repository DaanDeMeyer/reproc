#pragma once

#include <reproc++/detail/array.hpp>
#include <reproc++/detail/type_traits.hpp>

namespace reproc {

class environment : public detail::array {
public:
  environment(const char *const *envp = nullptr) // NOLINT
      : detail::array(envp, false)
  {}

  /*!
  `Environment` must be iterable as a sequence of string pairs. Examples of
  types that satisfy this requirement are `std::vector<std::pair<std::string,
  std::string>>` and `std::map<std::string, std::string>`.

  The pairs in `environment` represent the environment variables of the child
  process and are converted to the right format before being passed as the
  environment to `reproc_start`.

  Note that passing an empty container to this method will start the child
  process with no environment. To have the child process inherit the environment
  of the parent process, call the default constructor instead.
  */
  template <typename Environment,
            typename = detail::enable_if_not_char_array<Environment>>
  environment(const Environment &environment) // NOLINT
      : detail::array(from(environment), true)
  {}

private:
  template <typename Environment>
  static const char *const *from(const Environment &environment);
};

template <typename Environment>
const char *const *environment::from(const Environment &environment)
{
  using name_size_type =
      typename Environment::value_type::first_type::size_type;
  using value_size_type =
      typename Environment::value_type::second_type::size_type;

  const char **envp = new const char *[environment.size() + 1];
  std::size_t current = 0;

  for (const auto &entry : environment) {
    const auto &name = entry.first;
    const auto &value = entry.second;

    // We add 2 to the size to reserve space for the '=' sign and the null
    // terminator at the end of the string.
    char *string = new char[name.size() + value.size() + 2];

    envp[current++] = string;

    for (name_size_type i = 0; i < name.size(); i++) {
      *string++ = name[i];
    }

    *string++ = '=';

    for (value_size_type i = 0; i < value.size(); i++) {
      *string++ = value[i];
    }

    *string = '\0';
  }

  envp[current] = nullptr;

  return envp;
}

}

#pragma once

#include <reproc++/detail/array.hpp>
#include <reproc++/detail/type_traits.hpp>

namespace reproc {

class arguments : public detail::array {
public:
  arguments(const char *const *arguments) // NOLINT
      : detail::array(arguments, false)
  {}

  /*!
  `Arguments` must be a `Container` containing `SequenceContainer` containing
  characters. Examples of types that satisfy this requirement are
  `std::vector<std::string>` and `std::array<std::string>`.

  `arguments` has the same restrictions as `argv` in `reproc_start` except
  that it should not end with `NULL` (`start` allocates a new array which
  includes the missing `NULL` value).
  */
  template <typename Arguments,
            typename = detail::enable_if_not_char_array<Arguments>>
  arguments(const Arguments &arguments); // NOLINT
};

template <typename Arguments, typename>
arguments::arguments(const Arguments &arguments) : detail::array(nullptr, false)
{
  using size_type = typename Arguments::value_type::size_type;

  const char **data = new const char *[arguments.size() + 1];
  std::size_t current = 0;

  for (const auto &argument : arguments) {
    char *string = new char[argument.size() + 1];

    data[current++] = string;

    for (size_type i = 0; i < argument.size(); i++) {
      *string++ = argument[i];
    }

    *string = '\0';
  }

  data[current] = nullptr;

  this->data(data, true);
}

} // namespace reproc

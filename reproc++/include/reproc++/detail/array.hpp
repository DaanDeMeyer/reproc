#pragma once

#include <reproc++/export.hpp>

namespace reproc {
namespace detail {

class array {
  const char *const *data_;
  bool owned_;

public:
  REPROCXX_EXPORT array(const char *const *data, bool owned) noexcept;

  REPROCXX_EXPORT array(array &&other) noexcept;
  REPROCXX_EXPORT array &operator=(array &&other) noexcept;

  REPROCXX_EXPORT ~array() noexcept;

  REPROCXX_EXPORT const char *const *data() const noexcept;
};

} // namespace detail
} // namespace reproc

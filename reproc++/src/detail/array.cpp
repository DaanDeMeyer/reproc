#include <reproc++/detail/array.hpp>

#include <cstddef>

namespace reproc {
namespace detail {

array::array(const char *const *data, bool owned) noexcept
    : data_(data), owned_(owned)
{}

array::array(array &&other) noexcept : data_(other.data_), owned_(other.owned_)
{
  other.data_ = nullptr;
  owned_ = false;
}

array &array::operator=(array &&other) noexcept {
  if (&other != this) {
    data_ = other.data_;
    owned_ = other.owned_;
    other.data_ = nullptr;
    owned_ = false;
  }

  return *this;
}

array::~array() noexcept
{
  if (owned_) {
    for (size_t i = 0; data_[i] != nullptr; i++) {
      delete[] data_[i];
    }

    delete[] data_;
  }
}

const char *const *array::data() const noexcept
{
  return data_;
}

void array::data(const char *const *data, bool owned) noexcept
{
  data_ = data;
  owned_ = owned;
}

} // namespace detail
} // namespace reproc

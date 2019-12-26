#pragma once

namespace reproc {
namespace detail {

class array {
  const char *const *data_;
  bool owned_;

public:
  array(const char *const *data, bool owned) noexcept
      : data_(data), owned_(owned)
  {}

  array(array &&other) noexcept : data_(other.data_), owned_(other.owned_)
  {
    other.data_ = nullptr;
    owned_ = false;
  }

  array &operator=(array &&other) noexcept
  {
    if (&other != this) {
      data_ = other.data_;
      owned_ = other.owned_;
      other.data_ = nullptr;
      owned_ = false;
    }

    return *this;
  }

  ~array() noexcept
  {
    if (owned_) {
      for (unsigned int i = 0; data_[i] != nullptr; i++) {
        delete[] data_[i];
      }

      delete[] data_;
    }
  }

  const char *const *data() const noexcept
  {
    return data_;
  }
};

} // namespace detail
} // namespace reproc

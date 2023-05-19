#pragma once

#include <istream>
#include <string>
#include <vector>

#include <reproc++/reproc.hpp>

namespace reproc {

/*!
`reproc_fill` but takes lambdas as fillers. Return an error code from a filler
to break out of `filler` early. Set `written` to the number of bytes written to
buffer. Set `more` to false when more streaming. `in` expects the following
signature:

```c++
std::error_code filler(uint8_t *const buffer, const size_t bufSize, size_t&
written, bool& more);
```
This is to be used for sending stdin *after* the process is started,
which may be required if the data is too big to fit in the options
pipe that is filled before the process starts
*/
template <typename In>
std::error_code fill(process &process, In &&in, const size_t bufSize = 4096)
{
  bool more = true;
  std::error_code ec;

  std::vector<uint8_t> buffer;
  buffer.resize(bufSize);

  while (more) {
    size_t writeSize = 0;
    in(buffer.data(), bufSize, writeSize, more);

    size_t total_written = 0;
    while (total_written < writeSize) {
      int events = 0;
      std::tie(events, ec) = process.poll(event::in, infinite);
      if (ec) {
        return ec;
      }

      if ((events & event::deadline) != 0) {
        return std::make_error_code(std::errc::timed_out);
      }

      size_t written = 0;
      std::tie(written, ec) = process.write(buffer.data() + total_written,
                                            writeSize - total_written);

      if (ec) {
        return ec;
      }

      if (written == 0 && !ec) {
        return std::make_error_code(std::errc::io_error);
      }

      total_written += written;
    }
  }

  return ec;
}

namespace filler {

/*! Writes all input from `string`. */
class string {
  const std::string &string_;
  size_t offset_{ 0 };

public:
  explicit string(const std::string &string) noexcept : string_(string) {}

  std::error_code operator()(uint8_t *const buffer,
                             const size_t bufSize,
                             size_t &written,
                             bool &more)
  {
    if (offset_ >= string_.size()) {
      written = 0;
      more = false;
      return {};
    }

    const char *sdata{ string_.data() + offset_ };
    written = std::min(string_.size() - offset_, bufSize);

    std::copy(sdata, sdata + written, buffer);
    offset_ += written;
    return {};
  }
};

/*! Writes all input from `istream`. */
class istream {
  std::istream &istream_;

public:
  explicit istream(std::istream &istream) noexcept : istream_(istream) {}

  std::error_code operator()(uint8_t *const buffer,
                             const size_t bufSize,
                             size_t &written,
                             bool &more)
  {
    istream_.read(reinterpret_cast<char *const>(buffer),
                  static_cast<std::streamsize>(bufSize));
    written = static_cast<std::size_t>(istream_.gcount());

    if (istream_.bad() || (istream_.fail() && !istream_.eof()))
      return std::make_error_code(std::errc::operation_canceled);

    more = !istream_.eof();
  }
};

}
}

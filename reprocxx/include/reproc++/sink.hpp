#pragma once

#include <reproc++/reproc.hpp>

#include <iosfwd>
#include <mutex>
#include <ostream>
#include <string>

namespace reproc {
namespace sink {

/*! Reads all output into `string`. */
class string {
  std::string &string_;

public:
  explicit string(std::string &string) noexcept : string_(string) {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    (void) stream;
    string_.append(reinterpret_cast<const char *>(buffer), size);
    return true;
  }
};

/*! Forwards all output to `ostream`. */
class ostream {
  std::ostream &ostream_;

public:
  explicit ostream(std::ostream &ostream) noexcept : ostream_(ostream) {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    (void) stream;
    ostream_.write(reinterpret_cast<const char *>(buffer),
                   static_cast<std::streamsize>(size));
    return true;
  }
};

/*! Discards all output. */
class discard {
public:
  bool operator()(stream stream, const uint8_t *buffer, size_t size) noexcept
  {
    (void) stream;
    (void) buffer;
    (void) size;

    return true;
  }
};

namespace thread_safe {

/*! `sink::string` but locks the given mutex before invoking the sink. */
class string {
  sink::string sink_;
  std::mutex &mutex_;

public:
  string(std::string &string, std::mutex &mutex) noexcept
      : sink_(string), mutex_(mutex)
  {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return sink_(stream, buffer, size);
  }
};

}

}
}

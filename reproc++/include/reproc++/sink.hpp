#pragma once

#include <reproc++/export.hpp>
#include <reproc++/reproc.hpp>

#include <iosfwd>
#include <mutex>
#include <string>

namespace reproc {
namespace sink {

/*! Reads all output into `out`. */
class string {
  std::string &out_;
  std::string &err_;

public:
  REPROCXX_EXPORT explicit string(std::string &out, std::string &err) noexcept;

  REPROCXX_EXPORT bool
  operator()(stream stream, const uint8_t *buffer, unsigned int size);
};

/*! Forwards all output to `out`. */
class ostream {
  std::ostream &out_;
  std::ostream &err_;

public:
  REPROCXX_EXPORT explicit ostream(std::ostream &out,
                                   std::ostream &err) noexcept;

  REPROCXX_EXPORT bool
  operator()(stream stream, const uint8_t *buffer, unsigned int size);
};

/*! Discards all output. */
class discard {
public:
  REPROCXX_EXPORT bool
  operator()(stream stream, const uint8_t *buffer, unsigned int size) noexcept;
};

namespace thread_safe {

class string {
  sink::string sink_;
  std::mutex &mutex_;

public:
  REPROCXX_EXPORT
  string(std::string &out, std::string &err, std::mutex &mutex) noexcept;

  REPROCXX_EXPORT bool
  operator()(stream stream, const uint8_t *buffer, unsigned int size);
};

} // namespace thread_safe

} // namespace sink
} // namespace reproc

#pragma once

#include <reproc++/export.hpp>
#include <reproc++/reproc.hpp>

#include <cstdint>
#include <iosfwd>
#include <string>

namespace reproc {
namespace sink {

/*! Reads all output into `out`. */
class string {
  std::string &out_;
  std::string &err_;

public:
  REPROCXX_EXPORT explicit string(std::string &out,
                                  std::string &err) noexcept;

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

} // namespace sink
} // namespace reproc

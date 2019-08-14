#pragma once

#include <reproc++/export.hpp>

#include <cstdint>
#include <iosfwd>
#include <string>

namespace reproc {
namespace sink {

/*! Reads all output into `out`. */
class string {
  std::string &out_;

public:
  REPROCXX_EXPORT explicit string(std::string &out) noexcept;

  REPROCXX_EXPORT bool operator()(const uint8_t *buffer, unsigned int size);
};

/*! Forwards all output to `out`. */
class ostream {
  std::ostream &out_;

public:
  REPROCXX_EXPORT explicit ostream(std::ostream &out) noexcept;

  REPROCXX_EXPORT bool operator()(const uint8_t *buffer, unsigned int size);
};

/* Discards all output. */
class discard {
public:
  REPROCXX_EXPORT explicit discard() = default;

  REPROCXX_EXPORT bool operator()(const uint8_t *buffer,
                                  unsigned int size) noexcept;
};

} // namespace sink
} // namespace reproc

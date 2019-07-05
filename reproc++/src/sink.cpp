#include <reproc++/sink.hpp>

#include <ostream>

namespace reproc {

string_sink::string_sink(std::string &out) noexcept : out_(out) {}

bool string_sink::operator()(const uint8_t *buffer, unsigned int size)
{
  out_.append(reinterpret_cast<const char *>(buffer), size);
  return true;
}

ostream_sink::ostream_sink(std::ostream &out) noexcept : out_(out) {}

bool ostream_sink::operator()(const uint8_t *buffer, unsigned int size)
{
  out_.write(reinterpret_cast<const char *>(buffer), size);
  return true;
}

} // namespace reproc

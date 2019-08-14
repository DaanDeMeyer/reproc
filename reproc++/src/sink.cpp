#include <reproc++/sink.hpp>

#include <ostream>

namespace reproc {
namespace sink {

string::string(std::string &out) noexcept : out_(out) {}

bool string::operator()(const uint8_t *buffer, unsigned int size)
{
  out_.append(reinterpret_cast<const char *>(buffer), size);
  return true;
}

ostream::ostream(std::ostream &out) noexcept : out_(out) {}

bool ostream::operator()(const uint8_t *buffer, unsigned int size)
{
  out_.write(reinterpret_cast<const char *>(buffer), size);
  return true;
}

bool discard::operator()(const uint8_t *buffer, unsigned int size) noexcept
{
  (void) buffer;
  (void) size;
  return true;
}

} // namespace sink
} // namespace reproc

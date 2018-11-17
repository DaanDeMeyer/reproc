#include "reproc++/sink.hpp"

#include <ostream>

namespace reproc
{

string_sink::string_sink(std::string &out) noexcept : out_(out) {}

void string_sink::operator()(const char *buffer, unsigned int size)
{
  out_.append(buffer, size);
}

ostream_sink::ostream_sink(std::ostream &out) noexcept : out_(out) {}

void ostream_sink::operator()(const char *buffer, unsigned int size)
{
  out_.write(buffer, size);
}

} // namespace reproc

#include "reproc/parser.hpp"

#include <ostream>

namespace reproc
{

string_parser::string_parser(std::string &out) noexcept : out_(out) {}

bool string_parser::operator()(const char *buffer, unsigned int size)
{
  out_.append(buffer, size);
  return true;
}

ostream_parser::ostream_parser(std::ostream &out) noexcept : out_(out) {}

bool ostream_parser::operator()(const char *buffer, unsigned int size)
{
  out_.write(buffer, size);
  return true;
}

} // namespace reproc

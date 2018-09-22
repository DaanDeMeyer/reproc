/*! \file parser.hpp */

#ifndef REPROC_PARSER_HPP
#define REPROC_PARSER_HPP

#include <ostream>

/*! \namespace reproc */
namespace reproc
{

/*!
Reads the entire output of a child process into the given string.

\see \ref reproc::process::read
*/
class string_parser
{
  std::string &out_;

public:
  string_parser(std::string &out) noexcept : out_(out) {}

  bool operator()(const char *buffer, unsigned int size)
  {
    out_.append(buffer, size);
    return true;
  }
};

/*!
Forwards the entire output of a child process to the given output stream.

\see \ref reproc::process::read
*/
class ostream_parser
{
  std::ostream &out_;

public:
  ostream_parser(std::ostream &out) noexcept : out_(out) {}

  bool operator()(const char *buffer, unsigned int size)
  {
    out_.write(buffer, size);
    return true;
  }
};

} // namespace reproc

#endif

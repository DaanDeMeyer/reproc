/*! \file parser.hpp */

#ifndef REPROC_PARSER_HPP
#define REPROC_PARSER_HPP

#include <iosfwd>

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
  string_parser(std::string &out) noexcept;

  bool operator()(const char *buffer, unsigned int size);
};

/*!
Forwards the entire output of a child process to the given output stream.

\see \ref reproc::process::read
*/
class ostream_parser
{
  std::ostream &out_;

public:
  ostream_parser(std::ostream &out) noexcept;

  bool operator()(const char *buffer, unsigned int size);
};

} // namespace reproc

#endif

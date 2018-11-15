/*! \file parser.hpp */

#ifndef REPROC_PARSER_HPP
#define REPROC_PARSER_HPP

#include "reproc++/export.hpp"

#include <iosfwd>
#include <string>

/*! \namespace reproc */
namespace reproc
{

/*!
Reads the entire output of a child process into the given string.

\see \ref process::read
*/
class string_parser
{
  std::string &out_;

public:
  REPROC_EXPORT string_parser(std::string &out) noexcept;

  REPROC_EXPORT bool operator()(const char *buffer, unsigned int size);
};

/*!
Forwards the entire output of a child process to the given output stream.

\see \ref process::read
*/
class ostream_parser
{
  std::ostream &out_;

public:
  REPROC_EXPORT ostream_parser(std::ostream &out) noexcept;

  REPROC_EXPORT bool operator()(const char *buffer, unsigned int size);
};

} // namespace reproc

#endif

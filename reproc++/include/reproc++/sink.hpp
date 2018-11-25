/*! \file sink.hpp */

#ifndef REPROC_SINK_HPP
#define REPROC_SINK_HPP

#include <reproc++/export.hpp>

#include <iosfwd>
#include <string>

/*! \namespace reproc */
namespace reproc
{

/*!
Reads the entire output of a child process into the given string.

\see \ref process::drain
*/
class string_sink
{
  std::string &out_;

public:
  REPROCXX_EXPORT string_sink(std::string &out) noexcept;

  REPROCXX_EXPORT void operator()(const char *buffer, unsigned int size);
};

/*!
Forwards the entire output of a child process to the given output stream.

\see \ref process::drain
*/
class ostream_sink
{
  std::ostream &out_;

public:
  REPROCXX_EXPORT ostream_sink(std::ostream &out) noexcept;

  REPROCXX_EXPORT void operator()(const char *buffer, unsigned int size);
};

} // namespace reproc

#endif

#ifndef REPROC_PARSER_HPP
#define REPROC_PARSER_HPP

#include <ostream>

namespace reproc {

class string_parser {
  std::string &out_;

public:
  string_parser(std::string &out) : out_(out) {}

  bool stream_closed_is_error() { return false; }

  bool operator()(const char *buffer, unsigned int size) {
    out_.append(buffer, size);
    return true;
  }
};

class ostream_parser {
  std::ostream &out_;

public:
  ostream_parser(std::ostream &out) : out_(out) {}

  bool stream_closed_is_error() { return false; }

  bool operator()(const char *buffer, unsigned int size) {
    out_.write(buffer, size);
    return true;
  }
};

} // namespace reproc

#endif

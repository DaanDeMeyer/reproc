#include <reproc/parser.hpp>
#include <reproc/reproc.hpp>

#include <iostream>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

/*! Uses the reproc C++ API to print CMake's help page */
int main()
{
  reproc::process cmake_help;
  std::error_code ec;

  std::vector<std::string> args = { "cmake", "--help" };

  ec = cmake_help.start(args);
  if (ec) { return fail(ec); }

  std::string output;

  // string_parser reads the output into a string
  ec = cmake_help.read(reproc::cout, reproc::string_parser(output));
  if (ec) { return fail(ec); }

  std::cout << output << std::flush;

  // You can also pass an ostream_parser to write the output directly to cerr
  ec = cmake_help.read(reproc::cerr, reproc::ostream_parser(std::cerr));
  if (ec) { return fail(ec); }

  unsigned int exit_status = 0;
  ec = cmake_help.wait(reproc::infinite, &exit_status);
  if (ec) { return fail(ec); }

  return static_cast<int>(exit_status);
}

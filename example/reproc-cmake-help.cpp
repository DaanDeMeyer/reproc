#include <reproc/parser.hpp>
#include <reproc/reproc.hpp>

#include <iostream>

/*! Uses the reproc C++ API to print CMake's help page */
int main()
{
  reproc::process cmake_help;
  reproc::error error = reproc::success;

  std::vector<std::string> args = { "cmake", "--help" };

  error = cmake_help.start(args);
  if (error) { return error; }

  std::string output;

  error = cmake_help.read(reproc::cout, reproc::string_parser(output));
  if (error) { return error; }

  std::cout << output << std::flush;

  // You can also pass an ostream such as std::cerr to read
  error = cmake_help.read(reproc::cerr, reproc::ostream_parser(std::cerr));
  if (error) { return error; }

  error = cmake_help.wait(reproc::infinite);
  if (error) { return error; }

  int exit_status;
  error = cmake_help.exit_status(&exit_status);
  if (error) { return error; }

  return exit_status;
}

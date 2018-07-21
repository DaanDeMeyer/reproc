#include <reproc/parser.hpp>
#include <reproc/reproc.hpp>

#include <iostream>

/*! Uses the reproc C++ API to print CMake's help page */
int main()
{
  using reproc::Reproc;

  Reproc reproc;
  reproc::Error error = reproc::SUCCESS;

  std::vector<std::string> args = { "cmake", "--help" };

  error = reproc.start(args);
  if (error) { return error; }

  std::string output;

  error = reproc.read(reproc::STDOUT, reproc::string_parser(output));
  if (error) { return error; }

  std::cout << output << std::flush;

  // You can also pass an ostream such as std::cerr to read
  error = reproc.read(reproc::STDERR, reproc::ostream_parser(std::cerr));
  if (error) { return error; }

  error = reproc.wait(reproc::INFINITE);
  if (error) { return error; }

  int exit_status;
  error = reproc.exit_status(&exit_status);
  if (error) { return error; }

  return exit_status;
}

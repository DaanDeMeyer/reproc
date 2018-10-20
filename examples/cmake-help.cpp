/*! \example cmake-help.cpp */

#include <reproc++/parser.hpp>
#include <reproc++/reproc.hpp>

#include <iostream>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

// Uses the reproc C++ API to print CMake's help page.
int main()
{
  // We can pass parameters to the constructor that will be passed to
  // process::stop in the destructor. cmake --help is a short lived command and
  // will always exit on its own so waiting indefinitely (reproc::infinite)
  // until it exits on its own (reproc::cleanup::wait) is sufficient to make
  // sure the process is stopped and cleaned up.
  reproc::process cmake_help(reproc::cleanup::wait, reproc::infinite);

  std::vector<std::string> args = { "cmake", "--help" };

  std::error_code ec = cmake_help.start(args);

  if (ec == reproc::errc::file_not_found) {
    std::cerr << "cmake not found. Make sure it's available from the PATH.";
    return 1;
  }

  if (ec) { return fail(ec); }

  std::string output;

  // string_parser reads the child process output into the given string.
  ec = cmake_help.read(reproc::stream::out, reproc::string_parser(output));
  if (ec) { return fail(ec); }

  std::cout << output << std::flush;

  // You can also pass an ostream_parser to write the output directly to an
  // output stream such as std::cerr.
  ec = cmake_help.read(reproc::stream::err, reproc::ostream_parser(std::cerr));
  if (ec) { return fail(ec); }

  // Even if we pass cleanup parameters to the constructor, we can still call
  // stop ourselves. This allows us to retrieve the exit status of the process
  // which is hard to get if the process is stopped in the destructor. The
  // process class has logic to prevent stop from being called in the destructor
  // if it has already been called by the user.
  unsigned int exit_status = 0;
  ec = cmake_help.stop(reproc::cleanup::wait, reproc::infinite, &exit_status);
  if (ec) { return fail(ec); }

  return static_cast<int>(exit_status);
}

#include <reproc/reproc.hpp>

#include <iostream>

/*! Uses the reproc C++ API to print CMake's help page */
int main()
{
  Reproc reproc;
  Reproc::Error error = Reproc::SUCCESS;

  std::vector<std::string> args = { "cmake", "--help" };

  error = reproc.start(args, nullptr);
  if (error) { return error; }

  std::string output;

  // The C++ wrapper has a convenient method for reading all output of a child
  // reproc to an output stream. We could directly pass std::cout here as well
  // but we use std::ostringstream since usually you will want the output of a
  // child reproc as a string.
  error = reproc.read_all(output);
  if (error) { return error; }

  std::cout << output << std::flush;

  // You can also pass an ostream such as std::cerr directory to read_all
  error = reproc.read_all_stderr(std::cerr);
  if (error) { return error; }

  error = reproc.wait(Reproc::INFINITE);
  if (error) { return error; }

  int exit_status;
  error = reproc.exit_status(&exit_status);
  if (error) { return error; }

  return exit_status;
}

#include <reproc/parser.hpp>
#include <reproc/reproc.hpp>

#include <iostream>

/*!
Forwards the passed arguments to reproc and prints the child process output
on stdout.

Example: "./forward cmake --help" will print CMake's help output.

This program can be used to verify that manually executing a command and
executing it with reproc give the same output
*/
int main(int argc, char *argv[])
{
  if (argc <= 1) {
    std::cerr << "No arguments provided. Example usage: ./forward echo test";
    return 1;
  }

  reproc::process forward;
  reproc::error error = reproc::success;

  error = forward.start(argc - 1, argv + 1);
  if (error) { return error; }

  forward.close(reproc::cin);

  error = forward.read(reproc::cout, reproc::ostream_parser(std::cout));
  if (error) { return error; }

  error = forward.read(reproc::cerr, reproc::ostream_parser(std::cerr));
  if (error) { return error; }

  unsigned int exit_status = 0;
  error = forward.wait(reproc::infinite, &exit_status);
  if (error) { return error; }

  return static_cast<int>(exit_status);
}

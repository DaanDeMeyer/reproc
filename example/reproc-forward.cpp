#include <reproc/parser.hpp>
#include <reproc/reproc.hpp>

#include <iostream>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

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
    std::cerr << "No arguments provided. Example usage: ./forward cmake --help";
    return 1;
  }

  reproc::process forward;
  std::error_code ec;

  ec = forward.start(argc - 1, argv + 1);
  if (ec) { return fail(ec); }

  forward.close(reproc::cin);

  ec = forward.read(reproc::cout, reproc::ostream_parser(std::cout));
  if (ec) { return fail(ec); }

  ec = forward.read(reproc::cerr, reproc::ostream_parser(std::cerr));
  if (ec) { return fail(ec); }

  unsigned int exit_status = 0;
  ec = forward.wait(reproc::infinite, &exit_status);
  if (ec) { return fail(ec); }

  return static_cast<int>(exit_status);
}

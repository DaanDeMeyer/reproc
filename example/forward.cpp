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
  using reproc::Reproc;

  Reproc reproc;
  Reproc::Error error = Reproc::SUCCESS;

  error = reproc.start(argc - 1, argv + 1);
  if (error) { return error; }

  error = reproc.close_stdin();
  if (error) { return error; }

  error = reproc.read_all(std::cout);
  if (error) { return error; }

  error = reproc.read_all_stderr(std::cerr);
  if (error) { return error; }

  error = reproc.wait(Reproc::INFINITE);
  if (error) { return error; }

  int exit_status = 0;
  error = reproc.exit_status(&exit_status);
  if (error) { return error; }

  return exit_status;
}

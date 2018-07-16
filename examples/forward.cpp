#include <process-lib/process.hpp>

#include <iostream>

/*!
Forwards the passed arguments to process-lib and prints the child process output
on stdout.

Example: "./forward cmake --help" will print CMake's help output.

This program can be used to verify that manually executing a command and
executing it with process-lib give the same output
*/
int main(int argc, char *argv[])
{
  using process_lib::Process;

  Process process;
  Process::Error error = Process::SUCCESS;

  error = process.start(argc - 1, (const char **) argv + 1, nullptr);
  if (error) { return error; }

  error = process.close_stdin();
  if (error) { return error; }

  error = process.read_all(std::cout);
  if (error) { return error; }

  error = process.read_all_stderr(std::cerr);
  if (error) { return error; }

  error = process.wait(Process::INFINITE);
  if (error) { return error; }

  int exit_status = 0;
  error = process.exit_status(&exit_status);
  if (error) { return error; }

  return exit_status;
}

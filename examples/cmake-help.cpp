#include <process-lib/process.hpp>

#include <iostream>
#include <sstream>

/*! Uses the process-lib C++ API to print CMake's help page */
int main()
{
  Process process;
  std::vector<std::string> args = { "cmake", "--help" };

  Process::Error error = Process::SUCCESS;

  error = process.start(args, nullptr);
  if (error) { return error; }

  std::ostringstream output{};

  // The C++ wrapper has a convenient method for reading all output of a child
  // process to an output stream. We could directly pass std::cout here as well
  // but we use std::ostringstream since usually you will want the output of a
  // child process as a string.
  error = process.read_all(output);
  if (error) { return error; }

  std::cout << output.str() << std::flush;

  error = process.wait(Process::INFINITE);
  if (error) { return error; }

  int exit_status;
  error = process.exit_status(&exit_status);
  if (error) { return error; }

  return exit_status;
}

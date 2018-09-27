/*! \example forward.cpp */

#include <reproc/parser.hpp>
#include <reproc/reproc.hpp>

#include <iostream>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

/*
Forwards the program arguments to a child process and prints its output on
stdout.

Example: "./forward cmake --help" will print CMake's help output.

This program can be used to verify that manually executing a command and
executing it with reproc give the same output.
*/
int main(int argc, char *argv[])
{
  if (argc <= 1) {
    std::cerr << "No arguments provided. Example usage: ./forward cmake --help";
    return 1;
  }

  // The destructor calls process::stop with the parameters we pass in the
  // constructor which saves us from having to make sure the process is always
  // stopped correctly.
  //
  // Any kind of process can be started with forward so we make sure the process
  // is cleaned up correctly by adding the reproc::cleanup::terminate flag
  // (sends SIGTERM (POSIX) or CTRL-BREAK (Windows) and waits 5 seconds). We
  // also add the reproc::cleanup::kill flag which sends SIGKILL (POSIX) or
  // calls TerminateProcess (Windows) if the process hasn't exited after 5
  // seconds and waits 5 more seconds.
  //
  // Note that the timout value of 5s is a maximum time to wait. If the process
  // exits earlier than the timeout the destructor will return immediately.
  reproc::process forward(reproc::cleanup::terminate | reproc::cleanup::kill,
                          5000);

  std::error_code ec = forward.start(argc - 1, argv + 1);
  if (ec == reproc::errc::file_not_found) {
    std::cerr << "Program not found. Make sure it's available from the PATH";
    return 1;
  } else if (ec) {
    return fail(ec);
  }

  // Some programs wait for the input stream to be closed before continuing so
  // we close it explicitly.
  forward.close(reproc::stream::in);

  // Pipe child process stdout output to std::cout of parent process.
  ec = forward.read(reproc::stream::out, reproc::ostream_parser(std::cout));
  if (ec) { return fail(ec); }

  // Pipe child process stderr output to std::cerr of parent process.
  ec = forward.read(reproc::stream::err, reproc::ostream_parser(std::cerr));
  if (ec) { return fail(ec); }

  // Call stop ourselves to get the exit_status. See the cmake-help example for
  // more info.
  unsigned int exit_status = 0;
  ec = forward.stop(reproc::cleanup::wait, reproc::infinite, &exit_status);
  if (ec) { return fail(ec); }

  return static_cast<int>(exit_status);
}

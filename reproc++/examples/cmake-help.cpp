#include <reproc++/reproc.hpp>
#include <reproc++/sink.hpp>

#include <iostream>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

// Uses reproc++ to print CMake's help page.
int main()
{
  /* A child process is managed by the process class. We can pass function
  pointers of process stop methods (process::wait, process::terminate and
  process::kill) to its constructor which will be called in its destructor to
  stop the process if it is still running. */

  /* cmake --help is a short lived command and will always exit on its own so
  waiting indefinitely until it exits on its own is sufficient to make sure the
  process is always stopped and cleaned up. */
  reproc::process cmake_help(reproc::wait, reproc::infinite);

  /* Args are not passed to the constructor so they don't have to be stored in
  the process class. While the C API requires the args to end with a NULL
  value, the process class has a start method that takes a vector of std::string
  and takes care of converting the vector into the array expected by the C API
  (including adding the NULL value at the end). */
  std::vector<std::string> args = { "cmake", "--help" };

  /* The process is not started in the constructor since this would force every
  class that uses the process class to use std::optional or heap allocation
  or something similar to avoid starting the process in its constructor if it
  wants to delay starting the process to a method call. */
  std::error_code ec = cmake_help.start(args);

  /* Since reproc++ is based on a C library (reproc) it also uses error codes to
  report errors to the user (adapted to C++'s std::error_code). If an exception
  is needed instead, you can convert std::error_code's to exceptions using
  std::system_error. */
  if (ec == reproc::errc::file_not_found) {
    std::cerr << "cmake not found. Make sure it's available from the PATH.";
    return 1;
  }

  if (ec) { return fail(ec); }

  std::string output;

  /* drain reads from the stream until it is closed or an error occurs. By
  providing it with a string sink all output is stored in our output string. */
  ec = cmake_help.drain(reproc::stream::out, reproc::string_sink(output));
  if (ec) { return fail(ec); }

  std::cout << output << std::flush;

  // You can also pass an ostream sink to write the output directly to an
  // output stream such as std::cerr.
  ec = cmake_help.drain(reproc::stream::err, reproc::ostream_sink(std::cerr));
  if (ec) { return fail(ec); }

  /* It's easy to define your own sinks as well. Take a look at sink.cpp in
  the repository to see how string_sink and ostream_sink are defined. The
  documentation of process::drain also provides more information on the
  requirements a sink should fulfill. */

  /* Even if we pass stop parameters to the constructor, we can still call the
  individual methods ourselves. This allows us to retrieve the exit status of
  the process which is hard to get if the process is stopped in the destructor.
  The process class has logic to prevent the stop methods from being called in
  the destructor if it the process has already been stopped before the
  destructor is called. */
  unsigned int exit_status = 0;
  ec = cmake_help.wait(reproc::infinite, &exit_status);
  if (ec) { return fail(ec); }

  return static_cast<int>(exit_status);
}

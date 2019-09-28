#include <reproc++/reproc.hpp>
#include <reproc++/sink.hpp>

#include <array>
#include <iostream>

static int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

// Uses reproc++ to print CMake's help page.
int main()
{
  // A child process is managed by the process class. We can pass arguments to
  // its constructor which will be passed to `process::stop` in its destructor
  // if the child process is still running by then.

  // cmake --help is a short lived command and will always exit on its own so
  // waiting indefinitely until it exits on its own is sufficient to make sure
  // the process is always stopped and cleaned up. Waiting indefinitely in the
  // destructor is the default behaviour.
  reproc::process cmake_help(reproc::cleanup::wait, reproc::infinite);

  // While `reproc_start` requires the args to end with a `NULL` value,
  // `process` has a start method that works with any container containing
  // strings and takes care of converting the vector into the array of
  // null-terminated strings expected by `reproc_start` (including adding the
  // `NULL` value at the end of the array).
  std::array<std::string, 2> argv = { "cmake", "--help" };

  // The child process is not started in the constructor since this would force
  // every class that uses the process class to use `std::optional`, heap
  // allocation or something similar to avoid starting the process in its
  // constructor if it wants to delay starting the process to a method call.
  std::error_code ec = cmake_help.start(argv);

  // reproc++ uses error codes to report errors to the user. If an exception is
  // needed instead, you can convert `std::error_code`'s to exceptions using
  // `std::system_error`.

  // System errors are converted to `std::error_code`'s of the system category.
  // These can be matched against using values from the `std::errc` error
  // condition. See https://en.cppreference.com/w/cpp/error/errc for more
  // information.
  if (ec == std::errc::no_such_file_or_directory) {
    std::cerr << "cmake not found. Make sure it's available from the PATH.";
    return 1;
  } else if (ec) {
    return fail(ec);
  }

  std::string output;

  // `process::drain` reads from a stream until it is closed or an error occurs.
  // Providing it with a `string_sink` makes it store all output is stored in
  // the string passed to the string sink.
  ec = cmake_help.drain(reproc::stream::out, reproc::sink::string(output));
  if (ec) {
    return fail(ec);
  }

  std::cout << output << std::flush;

  // You can also pass an `ostream_sink` to write the output directly to an
  // output stream such as `std::cerr`.
  ec = cmake_help.drain(reproc::stream::err, reproc::sink::ostream(std::cerr));
  if (ec) {
    return fail(ec);
  }

  // It's easy to define your own sinks as well. Take a look at `sink.hpp` in
  // the repository to see how `string_sink` and `ostream_sink` are declared.
  // The documentation of `process::drain` also provides more information on the
  // requirements a sink should fulfill.

  // Even if we pass stop parameters to the constructor, we can still call the
  // individual stop methods ourselves. This allows us to retrieve the exit
  // status of the process which is impossible to retrieve if the process is
  // stopped in the destructor. `process` has logic to prevent `process::stop`
  // from being called in the destructor if the child process has already been
  // stopped before the destructor is called.
  ec = cmake_help.wait(reproc::infinite);
  if (ec) {
    return fail(ec);
  }

  return static_cast<int>(cmake_help.exit_status());
}

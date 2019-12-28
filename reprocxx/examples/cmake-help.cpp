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
  reproc::process process;

  // The `process::start` method works with any container containing strings and
  // takes care of converting the vector into the array of null-terminated
  // strings expected by `reproc_start` (including adding the `NULL` value at
  // the end of the array).
  std::array<std::string, 2> argv = { "cmake", "--help" };

  // reproc++ uses error codes to report errors. If exceptions are preferred,
  // convert `std::error_code`'s to exceptions using `std::system_error`.
  std::error_code ec = process.start(argv);

  // reproc++ converts system errors to `std::error_code`'s of the system
  // category. These can be matched against using values from the `std::errc`
  // error condition. See https://en.cppreference.com/w/cpp/error/errc for more
  // information.
  if (ec == std::errc::no_such_file_or_directory) {
    std::cerr << "cmake not found. Make sure it's available from the PATH.";
    return 1;
  } else if (ec) {
    return fail(ec);
  }

  // `process::drain` reads from the stdout and stderr streams of the child
  // process until both are closed or an error occurs. Providing it with a
  // string sink makes it store all output in the string(s) passed to the string
  // sink. Passing the same string to both the `out` and `err` arguments of
  // `sink::string` causes the stdout and stderr output to get stored in the
  // same string.
  std::string output;
  ec = process.drain(reproc::sink::string(output, output));
  if (ec) {
    return fail(ec);
  }

  std::cout << output << std::flush;

  // It's easy to define your own sinks as well. Take a look at `sink.cpp` in
  // the repository to see how `sink::string` and other sinks are implemented.
  // The documentation of `process::drain` also provides more information on the
  // requirements a sink should fulfill.

  // By default, The `process` destructor waits indefinitely for the child
  // process to exit to ensure proper cleanup. See the forward example for
  // information on how this can be configured. However, when relying on the
  // `process` destructor, we cannot check the exit status of the process so it
  // usually makes sense to explicitly wait for the process to exit and check
  // its exit status.

  int status = 0;
  std::tie(status, ec) = process.wait(reproc::infinite);
  if (ec) {
    return fail(ec);
  }

  return status;
}

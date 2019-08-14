#include <reproc++/reproc.hpp>
#include <reproc++/sink.hpp>

#include <future>
#include <iostream>

int fail(std::error_code ec)
{
  std::cerr << ec.message();
  return 1;
}

/*
See the cmake-help example for a completely documented C++ example. We only
thorougly document specifics in this example that are different from what's
already been explained in cmake-help.

The forward example forwards the program arguments to a child process and prints
its output on stdout.

Example: "./forward cmake --help" will print CMake's help output.

This program can be used to verify that manually executing a command and
executing it using reproc give the same output.
*/
int main(int argc, char *argv[])
{
  if (argc <= 1) {
    std::cerr << "No arguments provided. Example usage: ./forward cmake --help";
    return 1;
  }

  /*
  The destructor calls `process::stop` with the parameters we pass in the
  constructor which helps us make sure the process is always stopped correctly
  if unexpected errors happen.

  Any kind of process can be started with forward so we make sure the process
  is cleaned up correctly by specifying `reproc::terminate` which sends
  `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waits five seconds. We also
  add the `reproc::kill` flag which sends `SIGKILL` (POSIX) or calls
  `TerminateProcess` (Windows) if the process hasn't exited after five seconds
  and waits two more seconds for the child process to exit.

  Note that the timout values are maximum wait times. If the process exits
  earlier the destructor will return immediately.

  Also note that C++14 has chrono literals which allows
  `reproc::milliseconds(5000)` to be replaced with `5000ms`.
  */
  reproc::process forward(reproc::cleanup::terminate,
                          reproc::milliseconds(5000), reproc::cleanup::kill,
                          reproc::milliseconds(2000));

  std::error_code ec = forward.start(argv + 1);

  if (ec == std::errc::no_such_file_or_directory) {
    std::cerr << "Program not found. Make sure it's available from the PATH.";
    return 1;
  } else if (ec) {
    return fail(ec);
  }

  // Some programs wait for the input stream to be closed before continuing so
  // we close it explicitly.
  forward.close(reproc::stream::in);

  /* To avoid the child process hanging because the error stream is full while
  we're waiting for output from the output stream or vice-versa we spawn two
  separate threads to read from both streams at the same time. */

  // Pipe child process stdout output to stdout of the parent process.
  auto drain_stdout = std::async(std::launch::async, [&forward]() {
    return forward.drain(reproc::stream::out, reproc::sink::ostream(std::cout));
  });

  // Pipe child process stderr output to stderr of the parent process.
  auto drain_stderr = std::async(std::launch::async, [&forward]() {
    return forward.drain(reproc::stream::err, reproc::sink::ostream(std::cerr));
  });

  /* Call `process::stop` ourselves to get the exit status. We add
  `reproc::wait` with a timeout of ten seconds to give the process time to write
  its output before sending `SIGTERM`. */
  ec = forward.stop(reproc::cleanup::wait, reproc::milliseconds(10000),
                    reproc::cleanup::terminate, reproc::milliseconds(5000),
                    reproc::cleanup::kill, reproc::milliseconds(2000));
  if (ec) {
    return fail(ec);
  }

  ec = drain_stdout.get();
  if (ec) {
    return fail(ec);
  }

  ec = drain_stderr.get();
  if (ec) {
    return fail(ec);
  }

  return static_cast<int>(forward.exit_status());
}

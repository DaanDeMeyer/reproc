# reproc

- [What is reproc?](#what-is-reproc)
- [Features](#features)
- [Questions](#questions)
- [Installation](#installation)
- [Dependencies](#dependencies)
- [CMake options](#cmake-options)
- [Documentation](#documentation)
- [Error handling](#error-handling)
- [Multithreading](#multithreading)
- [Gotchas](#gotchas)

## What is reproc?

reproc (Redirected Process) is a cross-platform C/C++ library that simplifies
starting, stopping and communicating with external programs. The main use case
is executing command line applications directly from C or C++ code and
retrieving their output.

reproc consists out of two libraries: reproc and reproc++. reproc is a C99
library that contains the actual code for working with external programs.
reproc++ depends on reproc and adapts its API to an idiomatic C++11 API. It also
adds a few extras that simplify working with external programs from C++.

## Features

- Start any program directly from C or C++ code.
- Communicate with a program via its standard streams.
- Wait for a program to exit or forcefully stop it yourself. When forcefully
  stopping a process you can either allow the process to clean up its resources
  or stop it immediately.
- The core library (reproc) is written in C99. An optional C++11 wrapper library
  (reproc++) with extra features is available for use in C++ applications.
- Multiple installation methods. Either build reproc as part of your project or
  use a system installed version of reproc.

## Questions

If you have any questions after reading the readme and documentation you can
either make an issue or ask questions directly in the reproc
[gitter](https://gitter.im/reproc/Lobby) channel.

## Installation

**Note: Building reproc requires CMake 3.13 or higher.**

There are multiple ways to get reproc into your project. One way is to build
reproc as part of your project using CMake. To do this, we first have to get the
reproc source code into the project. This can be done using any of the following
options:

- When using CMake 3.11 or later, you can use the CMake `FetchContent` API to
  download reproc when running CMake. See
  <https://cliutils.gitlab.io/modern-cmake/chapters/projects/fetch.html> for an
  example.
- Another option is to include reproc's repository as a git submodule.
  <https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html>
  provides more information.
- A very simple solution is to just include reproc's source code in your
  repository. You can download a zip of the source code without the git history
  and add it to your repository in a separate directory.

After including reproc's source code in your project, it can be built from the
root CMakeLists.txt file as follows:

```cmake
add_subdirectory(<path-to-reproc>) # For example: add_subdirectory(external/reproc)
```

CMake options can be specified before calling `add_subdirectory`:

```cmake
set(REPROCXX ON)
add_subdirectory(<path-to-reproc>)
```

**Note: If the option has already been cached in a previous CMake run, you'll
have to clear CMake's cache to apply the new default value.**

For more information on configuring reproc's build, see
[CMake options](#cmake-options).

You can also depend on an installed version of reproc. You can either build and
install reproc yourself or install reproc via a package manager. reproc is
available in the following package repositories:

- Arch User Repository (<https://aur.archlinux.org/packages/reproc>)
- vcpkg (https://github.com/microsoft/vcpkg/tree/master/ports/reproc)

If using a package manager is not an option, you can build and install reproc
from source:

```sh
cmake -B build
cmake --build build
cmake --install build
```

After installing reproc your build system will have to find it. reproc provides
both CMake config files and pkg-config files to simplify finding a reproc
installation using CMake and pkg-config respectively. Note that reproc and
reproc++ are separate libraries and as a result have separate config files as
well. Make sure to search for the one you want to use.

To find an installed version of reproc using CMake:

```cmake
find_package(reproc) # Find reproc.
find_package(reprocxx) # Find reproc++.
```

After building reproc as part of your project or finding a installed version of
reproc, you can link against it from within your CMakeLists.txt file as follows:

```cmake
target_link_libraries(myapp reproc) # Link against reproc.
target_link_libraries(myapp reprocxx) # Link against reproc++.
```

## Dependencies

By default, reproc has a single dependency on pthreads on POSIX systems.
However, the dependency is included in both reproc's CMake config and pkg-config
files so it should be picked up by your build system automatically.

## CMake options

reproc's build can be configured using the following CMake options:

### User

- `REPROCXX`: Build reproc++ (default: `${REPROC_DEVELOP}`).
- `REPROC_TEST`: Build tests (default: `${REPROC_DEVELOP}`).

  Run the tests by running the `test` binary which can be found in the build
  directory after building reproc.

- `REPROC_EXAMPLES`: Build examples (default: `${REPROC_DEVELOP}`).

  The resulting binaries will be located in the examples folder of each project
  subdirectory in the build directory after building reproc.

### Advanced

- `REPROC_OBJECT_LIBRARIES`: Build CMake object libraries. Overrides
  `BUILD_SHARED_LIBS` (default: `${REPROC_DEVELOP}`).

  This is useful to directly include reproc in another library. When building
  reproc as a static or shared library, it has to be installed alongside the
  consuming library which makes distributing the consuming library harder. When
  using object libraries, reproc's object files are included directly into the
  consuming library and no extra installation is necessary.

- `REPROC_INSTALL`: Generate installation rules (default: `ON` unless
  `REPROC_OBJECT_LIBRARIES` is enabled).
- `REPROC_INSTALL_CMAKECONFIGDIR`: CMake config files installation directory
  (default: `${CMAKE_INSTALL_LIBDIR}/cmake`).
- `REPROC_INSTALL_PKGCONFIG`: Install pkg-config files (default: `ON`)
- `REPROC_INSTALL_PKGCONFIGDIR`: pkg-config files installation directory
  (default: `${CMAKE_INSTALL_LIBDIR}/pkgconfig`).

- `REPROC_MULTITHREADED`: Use `pthread_sigmask` and link against the system's
  thread library.

  `sigprocmask`'s behaviour is only defined for singlethreaded programs. When
  using multiple threads, `pthread_sigmask` should be used instead. Because we
  cannot determine whether reproc will be used in a multithreaded program when
  building reproc, `REPROC_MULTITHREADED` is enabled by default to guarantee
  defined behaviour. Users that know for certain their program will only use a
  single thread can opt to disable `REPROC_MULTITHREADED` when building reproc.

  When using reproc via CMake's `add_subdirectory` command and
  `REPROC_MULTITHREADED` is enabled, reproc will only call
  `find_package(Threads)` if the user has not called `find_package(Threads)`
  already. The `THREADS_PREFER_PTHREAD_FLAG` variable influences the behaviour
  of `find_package(Threads)`. if it is not defined already, reproc's build
  enables it before calling `find_package(Threads)`.

### Developer

- `REPROC_DEVELOP`: Configure option default values for development (default:
  `OFF` unless the `REPROC_DEVELOP` environment variable is set).
- `REPROC_SANITIZERS`: Build with sanitizers (default: `${REPROC_DEVELOP}`).
- `REPROC_TIDY`: Run clang-tidy when building (default: `${REPROC_DEVELOP}`).
- `REPROC_WARNINGS`: Enable compiler warnings (default: `${REPROC_DEVELOP}`)
- `REPROC_WARNINGS_AS_ERRORS`: Add -Werror or equivalent to the compile flags
  and clang-tidy (default: `OFF`).

## Documentation

Each function and class is documented extensively in its header file. Examples
can be found in the examples subdirectory of [reproc](reproc/examples) and
[reproc++](reprocxx/examples).

Be sure to read the gotchas section carefully to understand reproc's limitations
before using it.

## Error handling

On failure, Most functions in reproc's API return a negative `errno` (POSIX) or
`GetLastError` (Windows) style error code. For actionable errors, reproc
provides constants (`REPROC_ERROR_WAIT_TIMEOUT`, `REPROC_ERROR_STREAM_CLOSED`,
...) that can be used to match against the error without having to write
platform-specific code. To get a string representation of an error, pass it to
`reproc_error_string`.

```c
reproc_t *process = reproc_new();

int r = reproc_start(...);
if (r < 0) {
  goto cleanup;
}

r = reproc_write(...)
if (r == REPROC_ERROR_STREAM_CLOSED) {
  goto cleanup;
}

cleanup:
if (r < 0) {
  printf("%s\n", reproc_error_string(r));
}

return abs(r);
```

reproc++'s API integrates with the C++ standard library error codes mechanism
(`std::error_code` and `std::error_condition`). Most methods in reproc++'s API
return `std::error_code` values that contain the actual system error that
occurred. You can test against these error codes using the `std::errc` error
condition enum:

```c++
reproc::process;
std::error_code ec = process.start(...);

if (ec == std::errc::no_such_file_or_directory) {
  std::cerr << "Executable not found. Make sure it is available from the PATH.";
  return 1;
}

if (ec) {
  // Will print the actual system error value from errno or GetLastError() if a
  // system error occurred.
  std::cerr << ec.value() << std::endl;
  // You can also print a string representation of the error.
  std::cerr << ec.message();
  return 1;
}
```

reproc++ also provides aliases for relevant `std::errc` constants that are named
similarly to the error constants in reproc.

If needed, `std::error_code`'s can be converted to exceptions using
`std::system_error`:

```c++
reproc::process;
std::error_code ec = process.start(...);
if (ec) {
  throw std::system_error(ec, "Unable to start process");
}
```

## Multithreading

On POSIX, reproc should not be used from multiple threads at the same time. On
Windows, all operations on a child process should be done on the same thread
that the process was started from. See the gotchas section from more
information.

## Gotchas

- It is strongly recommended to block the `SIGCHLD` signal (POSIX) in all
  threads started in the parent process.

  On POSIX, reproc implements waiting for processes to exit by catching pending
  `SIGCHLD` signals. Because signals are only added to the pending signal queue
  when they are blocked by every thread, `reproc_wait` will fail to notice a
  child process exit if there are threads in the process that do not block
  `SIGCHLD` since the signal will be delivered to that thread. Because of this,
  it is strongly recommended to block the `SIGCHLD` signal in all threads
  started by the parent process.

- It is strongly recommended to have reproc manage all child processes in the
  parent process.

  On POSIX, even when the `SIGCHLD` signal is blocked in all threads, if two or
  more threads call `reproc_wait` at the same time, only one thread will receive
  any pending `SIGCHLD` signal. More generally, a pending `SIGCHLD` signal will
  be delivered to one of all threads that are waiting for pending `SIGCHLD`
  signals. As a result, any call to `reproc_wait` can block indefinitely if
  there are other threads catching pending `SIGCHLD` signals at the same time.
  Because of this, it is strongly recommended to only handle pending `SIGCHLD`
  signals from a single thread. If it is still viable to use reproc with that
  restriction, it follows that reproc should be responsible for catching all
  `SIGCHLD` signals and from that statement it follows that reproc should start
  all child processes in the parent process since reproc can only correctly
  clean up child processes that were started by reproc itself.

- It is strongly recommended to perform all operations on a child process on the
  same thread that the child process was started from.

  On Windows, reproc implements waiting for processes to exit by adding all the
  processes to a job object and adding a completion port to the job object. If
  we used a global completion port and job object, if two threads waited on
  notifications from the completion port, only one thread would actually get a
  notification and the other thread would remain blocked. To prevent this,
  reproc uses thread-local job objects and completion ports. Using this
  approach, multiple threads can wait for processes to exit at the same time. Of
  course, since a child process is only added to the job object of the thread it
  is started from, only the completion port on that thread will receive a
  notification when that child process exits. When waiting on a child process
  from a different thread than the one it was started from, reproc will wait
  indefinitely since the child process was never added to the job object of that
  thread and as a result the completion port of that thread will not receive a
  notification when that child process exits.

- It is strongly recommended to make sure each child process actually exits
  using `reproc_wait` or `reproc_stop`.

  On POSIX, a parent process is required to wait on a child process that has
  exited (using `reproc_wait`) before all resources related to that process can
  be released by the kernel. If the parent doesn't wait on a child process after
  it exits, the child process becomes a
  [zombie process](https://en.wikipedia.org/wiki/Zombie_process).

- It is strongly recommend to try terminating a child process by waiting for it
  to exit or by calling `reproc_terminate` before resorting to `reproc_kill`.

  When using `reproc_kill` the child process does not receive a chance to
  perform cleanup which could result in resources being leaked. Chief among
  these leaks is that the child process will not be able to stop its own child
  processes. Always try to let a child process exit normally by calling
  `reproc_terminate` before calling `reproc_kill`. `reproc_stop` is a handy
  helper function that can be used to perform multiple stop actions in a row
  with timeouts inbetween.

- It is strongly recommended to ignore the `SIGPIPE` signal in the parent
  process.

  On POSIX, writing to a closed stdin pipe of a child process will terminate the
  parent process with the `SIGPIPE` signal by default. To avoid this, the
  `SIGPIPE` signal has to be ignored in the parent process. If the `SIGPIPE`
  signal is ignored `reproc_write` will return `REPROC_ERROR_STREAM_CLOSED` as
  expected when writing to a closed stdin pipe.

- While `reproc_terminate` allows the child process to perform cleanup it is up
  to the child process to correctly clean up after itself. reproc only sends a
  termination signal to the child process. The child process itself is
  responsible for cleaning up its own child processes and other resources.

- (Windows) `reproc_kill` is not guaranteed to kill a child process immediately
  on Windows. For more information, read the Remarks section in the
  documentation of the Windows `TerminateProcess` function that reproc uses to
  kill child processes on Windows.

- While reproc tries its very best to avoid leaking file descriptors into child
  processes, there are scenarios where it cannot guarantee that no file
  descriptors will be leaked to child processes.

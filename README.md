# reproc <!-- omit in toc -->

**Be sure to select the git tag corresponding to the version you're using to get
the correct documentation. (For example:
<https://github.com/DaanDeMeyer/reproc/tree/v1.0.0> to get the v1.0.0
documentation)**

[![Build Status](https://travis-ci.com/DaanDeMeyer/reproc.svg?branch=master)](https://travis-ci.com/DaanDeMeyer/reproc)
[![Build status](https://ci.appveyor.com/api/projects/status/9d79srq8n7ytnrs5?svg=true)](https://ci.appveyor.com/project/DaanDeMeyer/reproc)
[![Join the chat at https://gitter.im/reproc/Lobby](https://badges.gitter.im/reproc/Lobby.svg)](https://gitter.im/reproc/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

- [What is reproc?](#what-is-reproc)
- [Features](#features)
- [Questions](#questions)
- [Getting started](#getting-started)
  - [Build reproc as part of your project using CMake](#build-reproc-as-part-of-your-project-using-cmake)
    - [FetchContent](#fetchcontent)
    - [Git submodule/vendor](#git-submodulevendor)
    - [Install](#install)
    - [CMake options](#cmake-options)
  - [Install from a Linux binary package](#install-from-a-linux-binary-package)
    - [Arch Linux](#arch-linux)
- [Documentation](#documentation)
- [Error handling](#error-handling)
- [Multithreading](#multithreading)
- [Gotcha's](#gotchas)
- [Design](#design)
  - [Memory allocation](#memory-allocation)
  - [(POSIX) Waiting on child process with timeout](#posix-waiting-on-child-process-with-timeout)
  - [(POSIX) Check if execve call was succesful](#posix-check-if-execve-call-was-succesful)
  - [Avoiding resource leaks](#avoiding-resource-leaks)
    - [POSIX](#posix)
    - [Windows](#windows)
- [Contributing](#contributing)

## What is reproc?

reproc (Redirected Process) is a cross-platform library that starts external
processes from within a C or C++ application, reads/writes to their
stdin/stdout/stderr streams and waits for them to exit or forcefully stops them.

The main use case is working with command line tools from within C or C++
applications.

## Features

- Start any program from within C or C++ code.
- Write to its standard input stream and read from its standard output and
  standard error streams.
- Wait for the program to exit or forcefully stop it yourself. When forcefully
  stopping a process you can optionally allow the process to clean up its
  resources or immediately stop it.
- The core library (reproc) is written in pure C. An optional C++ wrapper
  library (reproc++) with extra features is available for use in C++
  applications.
- Zero dependencies.
- Multiple installation methods. Either build reproc as part of your project or
  install it as a shared library.
- Care was taken to not leak resources anywhere in the library. See
  [Gotcha's](#gotchas) and [Avoiding resource leaks](##avoiding-resource-leaks)
  for more information.

## Questions

If you have any questions after reading the readme and documentation you can
either make an issue or ask questions directly in the reproc
[gitter](https://gitter.im/reproc/Lobby) channel.

## Getting started

reproc consists out of two libraries: reproc and reproc++. reproc is a C library
that contains all the low code for interacting with child processes. reproc++
depends on reproc and adapts its API to an idiomatic C++ API. It also adds a few
extra's that make working with child processes from C++ easier.

reproc can either be installed from a binary package or built as part of your
project when using CMake. All possible options are explained below.

### Build reproc as part of your project using CMake

#### FetchContent

If you're using CMake 3.11 or later you can use the
[FetchContent](https://cmake.org/cmake/help/v3.11/module/FetchContent.html) API
to use reproc in your project.

```cmake
include(FetchContent)

FetchContent_Declare(
  reproc
  GIT_REPOSITORY https://github.com/DaanDeMeyer/reproc.git
  GIT_TAG        v2.0.0-beta.2
)

FetchContent_GetProperties(reproc)
if(NOT reproc_POPULATED)
  FetchContent_Populate(reproc)

  # Configure reproc's build here. For example, to build reproc++ as well:
  set(REPROCXX ON CACHE BOOL "" FORCE)

  add_subdirectory(${reproc_SOURCE_DIR} ${reproc_BINARY_DIR})
endif()

add_executable(myapp myapp.c)
target_link_libraries(myapp reproc::reproc)
```

#### Git submodule/vendor

If you're using a CMake version older than 3.11, you can add reproc as a git
submodule instead:

```sh
# In your application source directory
mkdir external
cd external
git submodule add https://github.com/DaanDeMeyer/reproc.git
# Checkout a specific commit. This is usually a commit that corresponds to a
# Github release.
cd reproc
git checkout v2.0.0-beta.2 # Replace with latest commit or release tag
cd ../..
# Commit the result
git add .gitmodules external
git commit -m "Added reproc as a Git submodule"
```

The repository now has to be cloned with `git clone --recursive` instead of the
usual `git clone` to make sure all git submodules are pulled in as well.
`git submodule update --init` can be used to clone the git submodule in existing
clones of the repository.

We recommend against tracking the master branch when using git submodules
(instead of tracking a specific commit). This will result in the latest commit
being pulled in each time the submodule is cloned or updated. This can easily
lead to a different commits of reproc being used in separate clones of the
repository which could result in errors.

Updating the submodule is as simple as going into the submodule's root
directory, running `git checkout master` followed by `git pull` and checking out
the commit/release you want to update to.

If you're not using git you can download a zip/tar of the source code from
Github and manually put the code in a directory. To update you overwrite the
directory with the contents of an updated zip/tar from Github.

You can now call `add_subdirectory` in the root CMakeLists.txt file of your
application:

```cmake
add_subdirectory(external/reproc)
add_executable(myapp myapp.c)
target_link_libraries(myapp reproc::reproc)
```

#### Install

Installing is the way to go if you want to use reproc with other build systems
than CMake. After installing you can use your build systems preferred way of
finding libraries to find reproc. Refer to your build system's documentation for
more info. We give an example of how to find an installed version of reproc
using CMake.

First we have to build and install reproc:

```sh
git clone https://github.com/DaanDeMeyer/reproc.git
cd reproc
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
      -DREPROC_INSTALL=ON ..
# Might need root for this depending on install location
cmake --build . --target install
```

After installing reproc you can use `find_package` in the root CMakeLists.txt
file of your application to find reproc:

```cmake
find_package(reproc REQUIRED)
add_executable(myapp myapp.c)
target_link_libraries(myapp reproc::reproc)
```

This only finds reproc. To find reproc++:

```cmake
find_package(reproc++ REQUIRED)
add_executable(myapp myapp.cpp)
target_link_libraries(myapp reproc::reproc++)
```

`find_package(reproc++)` automatically finds reproc as its dependency so there's
no need for two separate `find_package` calls.

The install prefix specified when installing reproc might not be in the default
search path of CMake. If this is the case you can tell CMake where to search for
reproc as follows:

```sh
cmake -DCMAKE_PREFIX_PATH=<reproc-install-dir> .. # example: /usr/local on Linux
```

#### Leave the choice in the user's hands

If you're not sure which method to use you can also support multiple methods and
leave the choice of where to get reproc to the user:

```cmake
option(USE_SYSTEM_LIBRARIES "Use system installed libraries" OFF)

if(USE_SYSTEM_LIBRARIES)
  find_package(reproc REQUIRED)
# VERSION_GREATER_EQUAL requires CMake 3.7 or higher.
elseif(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.11)
  include(FetchContent)

  FetchContent_Declare(
    reproc
    GIT_REPOSITORY https://github.com/DaanDeMeyer/reproc.git
    GIT_TAG        v2.0.0-beta.2
  )

  FetchContent_GetProperties(reproc)
  if(NOT reproc_POPULATED)
    FetchContent_Populate(reproc)
    add_subdirectory(${reproc_SOURCE_DIR} ${reproc_BINARY_DIR})
  endif()
else()
  # Update or clone reproc git submodule.
  execute_process(COMMAND git submodule update --init -- external/reproc
                  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
  add_subdirectory(external/reproc)
endif()

add_executable(myapp myapp.cpp)
target_link_libraries(myapp reproc::reproc)
```

The above CMake code uses an installed system if `USE_SYSTEM_LIBRARIES=ON`. If
`USE_SYSTEM_LIBRARIES=OFF` and the user's CMake version is greater than 3.11, it
downloads reproc using CMake's `FetchContent` module. If `FetchContent` is not
available it falls back to git submodules.

#### CMake options

reproc supports the following CMake options:

- `REPROCXX (ON|OFF)`: Build reproc++ (default: `OFF`)

  If this option is enabled, both reproc and reproc++ will be built. After
  enabling this option and building reproc, link against reproc++ as follows:

  ```cmake
  target_link_libraries(myapp reproc::reproc++)
  ```

- `REPROC_BUILD_TESTS (ON|OFF)`: Build tests (default: `OFF`)
- `REPROC_BUILD_EXAMPLES (ON|OFF)`: Build examples (default: `OFF`)
- `REPROC_INSTALL (ON|OFF)`: Generate install target (default: `OFF`)

  Note that if you're using the default `CMAKE_INSTALL_PREFIX` on Windows,
  you'll have to set `REPROC_INSTALL=ON` the first time when running CMake to
  make sure the install directories are correct (This is because of
  `CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT` is only true on the first run of
  CMake in a new build tree).

- `BUILD_SHARED_LIBS (ON|OFF)`: Build reproc as a shared library (default:
  `OFF`)

  Note that when building reproc and reproc++ as shared libraries on Windows
  that you'll have to make sure reproc.dll and reproc++.dll are available from
  the PATH to run your application without crashing. Alternatively, you can also
  copy over the dll's manually to the same directory that your binary resides
  in. In general, unless you have a good reason not to, use reproc and reproc++
  as static libraries on Windows.

Options can be configured when building reproc or before calling
`add_subdirectory`:

- When building (in `<reproc-root-dir>/build` subdirectory):

  ```sh
  cmake -DREPROCXX=ON ..
  ```

- When using `add_subdirectory`:

  ```cmake
  set(REPROCXX ON CACHE BOOL "" FORCE)
  add_subdirectory(external/reproc)
  ```

### Install from a Linux binary package

#### Arch Linux

If you're using Arch Linux or any distro derived from it, you can install reproc
from the [AUR](https://aur.archlinux.org/packages/reproc).

## Documentation

API documentation and examples can be found at
<https://daandemeyer.github.io/reproc/>.

The latest docs can be built by enabling the cmake `REPROC_BUILD_DOCS` option
and building the `reproc-docs` target. This requires the latest version of
[Doxygen](https://www.stack.nl/~dimitri/doxygen/) to be installed and available
from the PATH. After building `reproc-docs` the generated documentation can be
found in the `docs/html` directory of the build directory.

```sh
mkdir build && cd build
cmake -DREPROC_BUILD_DOCS=ON ..
cmake --build . --target reproc-docs
firefox docs/html/index.html # Any installed browser should work
```

## Error handling

Most functions in the reproc C API return `REPROC_ERROR`. The `REPROC_ERROR`
represents all possible errors that can occur when calling reproc functions. Not
all errors apply to each function so the documentation includes a section
detailing which errors can occur in each function. One error that can be
returned by each function that returns `REPROC_ERROR` is `REPROC_UNKNOWN_ERROR`.
`REPROC_UNKNOWN_ERROR` is necessary because the documentation of the underlying
system calls reproc uses doesn't always detail what errors occur in which
circumstances (Windows is especially bad here).

To get more information when a reproc function returns `REPROC_UNKNOWN_ERROR`
reproc provides the `reproc_system_error` function that returns the actual
system error. Use this function to retrieve the actual system error and file an
issue with the system error and the reproc function that returned it. With this
information an extra value can be added to `REPROC_ERROR` and you'll be able to
check against this value instead of having to check against
`REPROC_UNKNOWN_ERROR`.

In the C++ API reproc's errors integrate with the C++ standard library error
codes (`std::error_code` and `std::error_condition`). All functions in the C++
API return `std::error_code` values that represent the actual system error if a
system error occurred. This means the `reproc_system_error` function is not
necessary in the C++ API since printing the value of the error code will print
the system error (in the C API printing the error value only gives you its value
in `REPROC_ERROR` and not the actual system error value). You can still test
against these error codes using the `reproc::errc` error condition enum:

```c++
reproc::process;
std::error_code ec = process.start(...);

if (ec == reproc::errc::file_not_found) {
  std::cerr << "Executable not found. Make sure it is available from the PATH";
  return 1;
}

if (ec) {
  // Will print the actual system error value from errno or GetLastError() if
  // error is a system error
  std::cerr << ec.value();
  // You can also print a string representation of the error
  std::cerr << ec.message();
  return 1;
}
```

Because reproc integrates with `std::error_code` you can also test against
reproc errors using values from the `std::errc` error condition enum:

```c++
reproc::process;
std::error_code ec = process.start(...);
if (ec == std::errc::not_enough_memory) {
  // handle error
}
```

If needed, you can also convert `std::error_code` values to exceptions using
`std::system_error`:

```c++
reproc::process;
std::error_code ec = process.start(...);
if (ec) { throw std::system_error(ec, "Unable to start process"); }
```

## Multithreading

Guidelines for using a reproc child process from multiple threads:

- Don't wait for or stop the same child process from multiple threads at the
  same time
- Don't read from or write to the same stream of the same child process from
  multiple threads at the same time

Different threads can read from or write to different streams at the same time.
This is a valid approach when you want to write to stdin and read from stdout in
parallel.

## Gotcha's

- (POSIX) On POSIX a parent process is required to wait on a child process
  (using `reproc_stop`) that has exited before all resources related to that
  process can be released by the kernel. If the parent doesn't wait on a child
  process after it exits, the child process becomes a
  [zombie process](https://en.wikipedia.org/wiki/Zombie_process).

- While `REPROC_TERMINATE` allows the child process to perform cleanup it is up
  to the child process to correctly clean up after itself. reproc only sends a
  termination signal to the child process. The child process itself is
  responsible for cleaning up its own child processes and other resources.

- When using `REPROC_KILL` the child process does not receive a chance to
  perform cleanup which could result in resources being leaked. Chief among
  these leaks is that the child process will not be able to stop its own child
  processes. Always let a child process exit normally or try to stop it with
  `REPROC_TERMINATE` before calling `REPROC_KILL`.

- (Windows) `REPROC_KILL` is not guaranteed to kill a child process on Windows.
  For more information, read the Remarks section in the documentation of the
  `TerminateProcess` function that reproc uses to kill child processes on
  Windows.

- (Windows) Immediately stopping a process (using either `REPROC_TERMINATE` or
  `REPROC_KILL`) after starting it on Windows might result in an error window
  with error code `0xc0000142` popping up. The error code indicates that the
  process was terminated before it was fully initialized. This problem shouldn't
  pop up with normal use of the library since most of the time you'll want to
  read/write to the process or wait until it exits normally.

  If someone runs into this problem, reproc's tests mitigate it by sleeping a
  few milliseconds before terminating the child process.

- File descriptors/handles created by reproc can leak to child processes not
  spawned by reproc if the application is multithreaded. This is not the case on
  systems that support the `pipe2` system call (Linux 2.6+ and newer BSD's).

  See [Avoiding resource leaks](#avoiding-resource-leaks) for more information.

- (POSIX) On POSIX platforms, file descriptors above the file descriptor
  resource limit (obtained with `sysconf(_SC_OPEN_MAX)`) and without the
  `FD_CLOEXEC` flag set are leaked into child processes created by reproc.

  Note that in multithreaded applications immediately setting the `FD_CLOEXEC`
  with `fcntl` after creating a file descriptor can still insufficient to avoid
  leaks.

  See [Avoiding resource leaks](#avoiding-resource-leaks) for more information.

- (Windows < Vista) File descriptors that are not marked not inheritable with
  `SetHandleInformation` will leak into reproc child processes.

  Note that the same `FD_CLOEXEC` caveat as mentioned above applies. In
  multithreaded applications there is a split moment after calling `CreatePipe`
  but before calling `SetHandleInformation` that a handle can still be inherited
  by reproc child processes.

  See [Avoiding resource leaks](#avoiding-resource-leaks) for more information.

- (POSIX) Writing to a closed stdin pipe of a child process will crash the
  parent process with the `SIGPIPE` signal. To avoid this the `SIGPIPE` signal
  has to be ignored in the parent process. If the `SIGPIPE` signal is ignored
  `reproc_write` will return `REPROC_STREAM_CLOSED` as expected when writing to
  a closed stdin pipe.

## Design

reproc is designed to be a minimal wrapper around the platform-specific API's
for starting a process, interacting with its standard streams and finally
terminating it. In this section we explain some design decisions as well as how
some parts of reproc work under the hood.

### Memory allocation

reproc aims to do as few dynamic memory allocations as possible in its own code
(not counting allocations that happen in system calls). As of this moment,
dynamic memory allocation in the C library is only done on Windows:

- When converting the array of program arguments to a single string as required
  by the `CreateProcess` function.
- When converting UTF-8 strings to UTF-16 (Windows Unicode functions take UTF-16
  strings as arguments).

Both these allocations happen in `process_start` and are freed before the
function returns.

I have not found a way to avoid allocating memory while keeping a uniform
cross-platform API for both POSIX and Windows. (Windows `CreateProcessW`
requires a single UTF-16 string of arguments delimited by spaces while POSIX
`execvp` requires an array of UTF-8 string arguments). Since reproc's API takes
child process arguments as an array of UTF-8 strings we have to allocate memory
to convert the array into a single UTF-16 string on Windows.

The reproc C code uses the standard `malloc` and `free` functions to allocate
and free memory. However, providing support for custom allocators should be
straightforward. If you need them, please open an issue.

In the C++ API, functions/methods that directly map to functions of the C API do
not do any extra allocations. Convenience functions/methods that do not appear
in the C API might do extra allocations in order to convert their arguments to
the format expected by the C API.

### (POSIX) Waiting on child process with timeout

I did not find a counterpart for the Windows `WaitForSingleObject` function
which can be used to wait until a process exits or the provided timeout expires.
POSIX has a similar function `waitpid` but this function does not support
specifying a timeout value.

To support waiting with a timeout value on POSIX, each process is put in its own
process group with the same id as the process id with a call to `setpgid` after
forking the process. When calling the `reproc_stop` function with a timeout
value between 0 and `REPROC_INFINITE`, a timeout process is forked which we put
in the same process group as the process we want to wait for with the same
`setpgid` function and puts itself to sleep for the requested amount of time
(timeout value) before exiting. We then call the `waitpid` function in the main
process but instead of passing the process id of the process we want to wait for
we pass the negative value of the process id. Passing a negative value for the
process id to `waitpid` instructs it to wait for all processes in the process
group of the absolute value of the passed negative value. In our case it will
wait for both the timeout process we started and the process we actually want to
wait for. If `waitpid` returns the process id of the timeout process we know the
timeout value has been exceeded. If `waitpid` returns the process id of the
process we want to wait for we know it has exited before the timeout process and
that the timeout value has not been exceeded.

This solution was inspired by [this](https://stackoverflow.com/a/8020324) Stack
Overflow answer.

### (POSIX) Check if execve call was succesful

reproc uses a fork-exec model to start new child processes on POSIX systems. A
problem that occured is that reproc needs to differentiate between errors that
happened before the exec call (which are errors from reproc) and errors after
the exec call (which are errors from the child process itself). To do this we
create an extra pipe in the parent procces with the `FD_CLOEXEC` flag set and
write any errors before and from exec to that pipe. If we then read from the
error pipe after forking the `read` call will either read 0 which means exec was
called and the write endpoint was closed (because of the `FD_CLOEXEC` flag) or
it reads a single integer (errno) which indicates an error occured before or
during exec.

This solution was inspired by [this](https://stackoverflow.com/a/1586277) Stack
Overflow answer.

### Avoiding resource leaks

#### POSIX

On POSIX systems, by default file descriptors are inherited by child processes
when calling `execve`. To prevent unintended leaking of file descriptors to
child processes, POSIX provides a function `fcntl` which can be used to set the
`FD_CLOEXEC` flag on file descriptors which instructs the underlying system to
close them when `execve` (or one of its variants) is called. However, using
`fcntl` introduces a race condition since any process created in another thread
after a file descriptor is created (for example using `pipe`) but before `fcntl`
is called to set `FD_CLOEXEC` on the file descriptor will still inherit that
file descriptor.

To get around this race condition reproc uses the `pipe2` function (when it is
available) which takes the `O_CLOEXEC` flag as an argument. This ensures the
file descriptors of the created pipe are closed when `execve` is called. Similar
system calls that take the `O_CLOEXEC` flag exist for other system calls that
create file descriptors. If `pipe2` is not available (for example on Darwin)
reproc falls back to calling `fcntl` to set `FD_CLOEXEC` immediately after
creating a pipe.

Darwin does not support the `FD_CLOEXEC` flag on any of its system calls but
instead provides an extra flag for the `posix_spawn` API (a wrapper around
`fork/exec`) named
[POSIX_SPAWN_CLOEXEC_DEFAULT](https://www.unix.com/man-page/osx/3/posix_spawnattr_setflags/)
that instructs `posix_spawn` to close all open file descriptors in the child
process created by `posix_spawn`. However, `posix_spawn` doesn't support
changing the working directory of the child process. A solution to get around
this was implemented in reproc but it was deemed too complex and brittle so it
was removed.

While using `pipe2` prevents file descriptors created by reproc from leaking
into other child processes, file descriptors created outside of reproc without
the `FD_CLOEXEC` flag set will still leak into reproc child processes. To mostly
get around this after forking and redirecting the standard streams (stdin,
stdout, stderr) of the child process we close all file descriptors (except the
standard streams) up to `_SC_OPEN_MAX` (obtained with `sysconf`) in the child
process. `_SC_OPEN_MAX` describes the maximum number of files that a process can
have open at any time. As a result, trying to close every file descriptor up to
this number closes all file descriptors of the child process which includes file
descriptors that were leaked into the child process. However, an application can
manually lower the resource limit at any time (for example with
`setrlimit(RLIMIT_NOFILE)`), which can lead to open file descriptors with a
value above the new resource limit if they were created before the resource
limit was lowered. These file descriptors will not be closed in the child
process since only the file descriptors up to the latest resource limit are
closed. Of course, this only happens if the application manually lowers the
resource limit.

#### Windows

On Windows the `CreatePipe` function receives a flag as part of its arguments
that specifies if the returned handles can be inherited by child processes or
not. The `CreateProcess` function also takes a flag indicating whether it should
inherit handles or not. Inheritance for endpoints of a single pipe can be
configured after the `CreatePipe` call using the function
`SetHandleInformation`. A race condition occurs after calling `CreatePipe`
(allowing inheritance) but before calling `SetHandleInformation` in one thread
and calling `CreateProcess` (configured to inherit pipes) in another thread. In
this scenario handles are unintentionally leaked into a child process. We try to
mitigate this in two ways:

- We call `SetHandleInformation` after `CreatePipe` for the handles that should
  not be inherited by any process to lower the chance of them accidentally being
  inherited (just like with `fnctl` if `µipe2` is not available). This only
  works for half of the endpoints created (the ones intended to be used by the
  parent process) since the endpoints intended to be used by the child actually
  need to be inherited by their corresponding child process.
- Windows Vista added the `STARTUPINFOEXW` structure in which we can put a list
  of handles that should be inherited. Only these handles are inherited by the
  child process. This again (just like Darwin's `posix_spawn`) only stops
  reproc's processes from inheriting unintended handles. Other code in an
  application that calls `CreateProcess` without passing a `STARTUPINFOEXW`
  struct containing the handles it should inherit can still unintentionally
  inherit handles meant for a reproc child process. reproc uses the
  `STARTUPINFOEXW` struct if it is available.

## Contributing

When making changes:

- Make sure all code in reproc still compiles by enabling all related CMake
  options:

  ```sh
  cmake -DREPROC_BUILD_TESTS=ON -DREPROC_BUILD_EXAMPLES=ON
  -DREPROCXX=ON .. # In build subdirectory
  ```

- Format your changes with clang-format and run clang-tidy locally since it will
  run in CI as well.

  If the `REPROC_CLANG_FORMAT` CMake option is enabled, the reproc-format target
  is added that formats all reproc source files with clang-format.

  Example usage: `cmake --build build --target reproc-format`

  If the `REPROC_CLANG_TIDY` CMake option is enabled, CMake will run clang-tidy
  on all reproc source files while building.

  If CMake can't find clang-format or clang-tidy you can tell it where to look:

  `cmake -DCMAKE_PREFIX_PATH=<clang-install-location> ..`

- Make sure all tests still pass. Tests can be run by executing
  `cmake --build build --target reproc-run-tests` in the root directory of the
  reproc repository.

  However, this method does not allow passing arguments to the tests executable.
  If you want more control over which tests are executed you can run the tests
  executable directly (located at `build/tests/tests`). A list of possible
  options can be found
  [here](https://github.com/onqtam/doctest/blob/master/doc/markdown/commandline.md).

  If you don't have access to every platform, make a pull request and CI will
  compile and run the tests on the platforms you don't have access to.

  Tests will be compiled with sanitizers in CI so make sure to not introduce any
  leaks or undefined behaviour. Enable compiling with sanitizers locally as
  follows:

  `cmake -DREPROC_SANITIZERS=ON ..`

- Enable `/W4` on Windows with `cmake -DREPROC_ENABLE_W4=ON ..`

- When adding a new feature, make sure to implement it for both POSIX and
  Windows.
- When adding a new feature, add a new test for it or modify an existing one to
  also test the new feature.
- Make sure to update the relevant documentation if needed or write new
  documentation.
- By default the output from the Ninja generator isn't colored. To get colored
  output when using the Ninja generator, enable the `REPROC_COLORED_OUTPUT`
  option:

  `cmake -DREPROC_COLORED_OUTPUT=ON ..`

- Make sure to follow the .editorconfig rules. Most editors have an editorconfig
  plugin to make this easy.
- reproc doesn't store a .gitignore file in the repository so everyone can add a
  custom .gitignore file locally. Example .gitignore file:

  ```txt
  # Exclude the .gitignore file itself so it isn't pushed to Github.
  .gitignore

  # Exclude the build directory.
  build/
  ```

# Reproc <!-- omit in toc -->

[![Build Status](https://travis-ci.com/DaanDeMeyer/reproc.svg?branch=master)](https://travis-ci.com/DaanDeMeyer/reproc)
[![Build status](https://ci.appveyor.com/api/projects/status/9d79srq8n7ytnrs5?svg=true)](https://ci.appveyor.com/project/DaanDeMeyer/reproc)
[![Join the chat at https://gitter.im/reproc/Lobby](https://badges.gitter.im/reproc/Lobby.svg)](https://gitter.im/reproc/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

- [Questions](#questions)
- [Installation](#installation)
  - [FetchContent](#fetchcontent)
  - [Git submodule/vendor](#git-submodulevendor)
  - [Install](#install)
  - [CMake user options](#cmake-user-options)
- [Usage](#usage)
- [Documentation](#documentation)
- [Unknown errors](#unknown-errors)
- [Gotcha's](#gotchas)
- [Design](#design)
  - [Opaque pointer](#opaque-pointer)
  - [Memory allocation](#memory-allocation)
  - [(POSIX) Waiting on child process with timeout](#posix-waiting-on-child-process-with-timeout)
  - [(POSIX) Check if execve call was succesful](#posix-check-if-execve-call-was-succesful)
  - [Avoiding resource leaks](#avoiding-resource-leaks)
- [Contributing](#contributing)
  - [CMake developer options](#cmake-developer-options)

## Questions

If you have any questions after reading the readme and documentation you can
either make an issue or ask questions directly in the reproc
[gitter](https://gitter.im/reproc/Lobby) channel.

## Getting started

To use reproc you'll have to compile it first. reproc can either be installed or
directly built as part of your project when using CMake. We explain all possible
options below.

### FetchContent

If you're using CMake 3.11 or later you can use the
[FetchContent](https://cmake.org/cmake/help/v3.11/module/FetchContent.html) API
to use reproc in your project.

```cmake
include(FetchContent)

FetchContent_Declare(
  REPROC
  GIT_REPOSITORY https://github.com/DaanDeMeyer/reproc.git
  GIT_TAG        origin/master
)

FetchContent_GetProperties(REPROC)
if(NOT REPROC_POPULATED)
  FetchContent_Populate(REPROC)
  add_subdirectory(${REPROC_SOURCE_DIR} ${REPROC_BINARY_DIR})
endif()

add_executable(myapp myapp.c)
target_link_libraries(myapp reproc::reproc)
```

### Git submodule/vendor

If you're using a CMake version older than 3.11, you can add reproc as a git
submodule instead:

```bash
# In your application source directory
mkdir third-party
cd third-party
git submodule add https://github.com/DaanDeMeyer/reproc.git
# Optionally checkout a specific commit. This is usually a commit that
# corresponds to a Github release.
cd reproc
git checkout 41f90d0
cd ../..
# Commit the result
git add third-party
git commit -m "Added reproc as a Git submodule"
```

If you're not using git you can download a zip/tar of the source code from
Github and manually put the code in a third-party directory. To update you
overwrite the directory with the contents of an updated zip/tar from Github.

You can now call `add_subdirectory` in the root CMakeLists.txt file of your
application:

```cmake
add_subdirectory(third-party/reproc)
add_executable(myapp myapp.c)
target_link_libraries(myapp reproc::reproc)
```

### Install

Installing is the way to go if you want to use reproc with other build systems
than CMake. After installing you can use your build systems preferred way of
finding libraries to find reproc. Refer to your build system's documentation for
more info. We give an example of how to find an installed version of reproc
using CMake.

First we have to build and install reproc:

```bash
git clone https://github.com/DaanDeMeyer/reproc.git
cd reproc
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build . --target install # Might need root for this
```

After installing reproc you can use `find_package` in the root CMakeLists.txt
file of your application to find reproc:

```cmake
find_package(reproc REQUIRED)
add_executable(myapp myapp.c)
target_link_libraries(myapp reproc::reproc)
```

The install prefix specified when installing reproc might not be in the default
search path of CMake. If this is the case you can tell CMake where to search for
reproc as follows:

```bash
cmake -DCMAKE_PREFIX_PATH=<reproc-install-dir> .. # example: /usr on Linux
```

### CMake user options

reproc supports the following CMake options:

- `REPROC_BUILD_CXX_WRAPPER (ON|OFF)`: Build the C++ wrapper (default: `OFF`)
- `REPROC_BUILD_TESTS (ON|OFF)`: Build tests (default: `OFF`)
- `REPROC_BUILD_EXAMPLES (ON|OFF)`: Build examples (default: `OFF`)
- `BUILD_SHARED_LIBS (ON|OFF)`: Build reproc as a static or shared library

Options can be configured when building reproc or before calling
`add_subdirectory`:

- When building (in `<reproc-root-dir>/build` subdirectory):
  `cmake -DREPROC_BUILD_CXX_WRAPPER=ON ..`

- When using `add_subdirectory`:

  ```cmake
  set(REPROC_BUILD_CXX_WRAPPER ON CACHE BOOL FORCE)
  add_subdirectory(third-party/reproc)
  ```

## Usage

See [example/git-status.c](example/cmake-help.c) for an example that uses reproc
to print the output of `git status`.
[example/cmake-help.cpp](example/cmake-help.cpp) prints the output of
`cmake --help` using the C++ API. [example/forward.cpp](example/forward.cpp)
spawns a child process using the provided command line arguments and prints its
output.

## Documentation

API documentation can be found in [reproc.h](include/c/reproc/reproc.h).
Documentation for the C++ wrapper can be found in
[reproc.hpp](include/cpp/reproc/reproc.hpp) (which mostly refers to reproc.h).
The C++ wrapper

## Unknown errors

There are lots of things that can go wrong when working with child processes.
reproc tries to unify the different platform errors as much as possible but this
is an ongoing effort. In particular, the Windows Win32 documentation mostly does
not specify what errors a function can throw. As a result, when an error occurs
on Windows reproc will usually return `REPROC_UNKNOWN_ERROR`. To get more
information reproc provides a function `process_system_error` which returns
actual system error. Use this function to retrieve the actual system error and
file an issue with the system error and the reproc function that returned it.
This way we can identify unknown errors and add them to reproc.

## Gotcha's

- (POSIX) On POSIX a parent process is required to wait on a child process
  (using `reproc_wait`) that has exited before all resources related to that
  process can be freed by the kernel. If the parent doesn't wait on a child
  process after it exits, the child process becomes a
  [zombie process](https://en.wikipedia.org/wiki/Zombie_process).

- While `reproc_terminate` allows the child process to perform cleanup it is up
  to the child process to correctly clean up after itself. reproc only sends a
  termination signal to the child process. The child process itself is
  responsible for cleaning up its own child processes and other resources.

- When using `reproc_kill` the child process does not receive a chance to
  perform cleanup which could result in resources being leaked. Chief among
  these leaks is that the child process will not be able to stop its own child
  processes. Always let a child process exit normally or try to stop it with
  `reproc_terminate` before calling `reproc_kill`.

- (Windows) `reproc_kill` is not guaranteed to kill a child process on Windows.
  For more information, read the Remarks section in the documentation of the
  `TerminateProcess` function that reproc uses to kill child processes on
  Windows.

- (Windows) Immediately stopping a process (using either `reproc_terminate` or
  `reproc_kill`) after starting it on Windows might result in an error window
  with error code `0xc0000142` popping up. The error code indicates that the
  process was terminated before it was fully initialized. This problem shouldn't
  pop up with normal use of the library since most of the time you'll want to
  read/write to the process or wait until it exits normally.

  If someone runs into this problem, reproc's tests mitigate it by waiting a few
  milliseconds using `reproc_wait` before terminating the child process.

- File descriptors/handles created by reproc can leak to child processes not
  spawned by reproc if the application is multithreaded. This is not the case on
  systems that support the `pipe2` system call (Linux 2.6+ and newer BSD's). See
  [Avoiding resource leaks](#avoiding-resource-leaks) for more information.

- (POSIX) On POSIX platforms, file descriptors above the file descriptor
  resource limit (obtained with `sysconf(_SC_OPEN_MAX)`) and without the
  `FD_CLOEXEC` flag set are leaked into child processes created by reproc.

  Note that in multithreaded applications immediately setting the `FD_CLOEXEC`
  with `fcntl` after creating a file descriptor can still not be sufficient to
  avoid leaks. See [Avoiding resource leaks](#avoiding-resource-leaks) for more
  information.

- (Windows < Vista) File descriptors that are not marked not inheritable with
  `SetHandleInformation` will leak into reproc child processes.

  Note that the same `FD_CLOEXEC` caveat as mentioned above applies. In
  multithreaded applications there is a split moment after calling `CreatePipe`
  but before calling `SetHandleInformation` that a handle can still be inherited
  by reproc child processes. See
  [Avoiding resource leaks](#avoiding-resource-leaks) for more information.

## Design

reproc is designed to be a minimal wrapper around the platform-specific API's
for starting a process, interacting with its standard streams and finally
terminating it. In this section we explain some design decisions as well as how
some parts of reproc work under the hood.

### Memory allocation

#### C API

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

#### C++ API

To avoid having to include reproc.h in reproc.hpp (the C++ API header) we have
to forward declare the `reproc` struct in the `Reproc` class of the C++ wrapper.
This means each instance of the `Reproc` class comes with at least one
allocation in its constructor to allocate memory for the `reproc` struct.

Aside from this, we can divide the methods of the `Reproc` class in two
categories:

- The methods that directly map to methods of the C api. These don't do any
  extra processing and thus do not allocate any memory (aside from allocations
  that happen in the C api).

- Convenience methods that are not available in the C api and are meant to make
  reproc easier to use from C++ (e.g. `read_all`). These methods work with STL
  types and do allocate memory.

### (POSIX) Waiting on child process with timeout

I did not find a counterpart for the Windows
[WaitForSingleObject](<https://msdn.microsoft.com/en-us/library/windows/desktop/ms687032(v=vs.85).aspx>)
function which can be used to wait until a process exits or the provided timeout
expires. POSIX has a similar function called
[waitpid](https://linux.die.net/man/2/waitpid) but this function does not
support specifying a timeout value.

To support waiting with a timeout value on POSIX, each process is put in its own
process group with the same id as the process id with a call to `setpgid` after
forking the process. When calling the `reproc_wait` function, a timeout process
is forked which we put in the same process group as the process we want to wait
for with the same `setpgid` function and puts itself to sleep for the requested
amount of time (timeout value) before exiting. We then call the `waitpid`
function in the main process but instead of passing the process id of the
process we want to wait for we pass the negative value of the process id
`-reproc->pid`. Passing a negative value for the process id to `waitpid`
instructs it to wait for all processes in the process group of the absolute
value of the passed negative value. In our case it will wait for both the
timeout process we started and the process we actually want to wait for. If
`waitpid` returns the process id of the timeout process we know the timeout
value has been exceeded. If `waitpid` returns the process id of the process we
want to wait for we know it has exited before the timeout process and that the
timeout value has not been exceeded.

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
  child process. This again (just like Darwin `posix_spawn`) only stops reproc's
  processes from inheriting unintended handles. Other code in an application
  that calls `CreateProcess` without passing a `STARTUPINFOEXW` struct
  containing the handles it should inherit can still unintentionally inherit
  handles meant for a reproc child process. reproc uses the `STARTUPINFOEXW`
  struct if it is available.

## Contributing

When making a pull request:

- Format your changes with `clang-format` and run `clang-tidy` locally if
  possible since it will run in CI as well.

  If `clang-format` is available in CMake's search path, a `format` target is
  added that can be used to run `clang-format` on all reproc source files.

  `cmake --build build --target format`

  reproc also supports the `REPROC_RUN_CLANG_TIDY` CMake option to run
  `clang-tidy` as part of the build. This requires `clang-tidy` to be in CMake's
  search path (PATH is included in CMake's search path).

- Make sure all tests still pass. Tests can be run by executing
  `cmake --build build --target test` in the root directory of the reproc
  repository.

  However, this method does not allow passing arguments to the tests executable.
  If you want more control over which tests are executed you can run the tests
  executable directly (located at `build/test/tests`). A list of possible
  options can be found
  [here](https://github.com/onqtam/doctest/blob/master/doc/markdown/commandline.md).

  If you don't have access to every platform, make a pull request and CI will
  compile and run the tests on the platforms you don't have access to.

- When adding a new feature, make sure to implement it for both POSIX and
  Windows.
- When adding a new feature, add a new test for it or modify an existing one to
  also test the new feature.
- Make sure to update the relevant documentation if needed or write new
  documentation.

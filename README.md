# process-lib <!-- omit in toc -->

[![Build Status](https://travis-ci.com/DaanDeMeyer/process-lib.svg?branch=master)](https://travis-ci.com/DaanDeMeyer/process-lib)
[![Build status](https://ci.appveyor.com/api/projects/status/nssmvol3nj683akq?svg=true)](https://ci.appveyor.com/project/DaanDeMeyer/process-lib)

- [Installation](#installation)
  - [FetchContent](#fetchcontent)
  - [Git Submodule](#git-submodule)
  - [Vendor](#vendor)
  - [CMake Options](#cmake-options)
- [Usage](#usage)
- [Documentation](#documentation)
- [Error Handling](#error-handling)
- [Gotcha's](#gotchas)
- [Design](#design)
  - [Opaque pointer](#opaque-pointer)
  - [Memory allocation](#memory-allocation)
  - [(POSIX) Waiting on child process with timeout](#posix-waiting-on-child-process-with-timeout)
  - [(POSIX) Check if execve call was succesful](#posix-check-if-execve-call-was-succesful)

## Installation

### FetchContent

The easiest way to use process-lib is with the CMake build system. If you're
using CMake 3.11 or later you can use the
[FetchContent](https://cmake.org/cmake/help/v3.11/module/FetchContent.html) api
to use process-lib in your project.

```cmake
include(FetchContent)

FetchContent_Declare(
  PROCESS_LIB
  GIT_REPOSITORY https://github.com/DaanDeMeyer/process-lib.git
  GIT_TAG        origin/master
)

FetchContent_GetProperties(PROCESS_LIB)
if(NOT PROCESS_LIB_POPULATED)
  FetchContent_Populate(PROCESS_LIB)
  add_subdirectory(${PROCESS_LIB_SOURCE_DIR} ${PROCESS_LIB_BINARY_DIR})
endif()

target_link_libraries(executable process)
```

### Git Submodule

If you can't use CMake 3.11 or higher, you can add process-lib as a git
submodule instead:

```sh
mkdir third-party
cd third-party
git submodule add https://github.com/DaanDeMeyer/process-lib.git
# Optionally checkout a specific commit
cd process-lib
git checkout 41f90d0
```

Commit the result and you can call `add_subdirectory` with the process-lib
directory:

```cmake
add_subdirectory(third-party/process-lib)

target_link_libraries(executable process)
```

### Vendor

If you're not using git you can download a zip/tar of the source code from
Github and manually put the code in a third-party directory. To update you
overwrite the process-lib directory with the contents of an updated zip/tar from
Github.

After unzipping the source code to third-party/process-lib you can then add the
directory with add_subdirectory in CMake.

```cmake
add_subdirectory(third-party/process-lib)

target_link_libraries(executable process)
```

### CMake Options

process-lib supports the following CMake options:

- `PROCESS_LIB_BUILD_CPP_WRAPPER (ON|OFF)`: Build the C++ wrapper (default:
  `OFF`)
- `PROCESS_LIB_BUILD_TESTS (ON|OFF)`: Build tests (default: `OFF`)
- `PROCESS_LIB_BUILD_EXAMPLES (ON|OFF)`: Build examples (default: `OFF`)

Options can be configured before calling `add_subdirectory` as follows:

```cmake
set(PROCESS_LIB_BUILD_CPP_WRAPPER ON CACHE BOOL FORCE)
```

## Usage

See [examples/cmake-help.c](examples/cmake-help.c) for an example that uses
process-lib to print the cmake CLI --help output.
[examples/cmake-help.cpp](examples/cmake-help.cpp) does the same but with the
C++ api.

## Documentation

API documentation can be found in [process.h](include/c/process.h).
Documentation for the C++ wrapper can be found in
[process.hpp](include/cpp/process.hpp) (which mostly refers to process.h).

## Error Handling

There are lots of things that can go wrong when working with child processes.
process-lib tries to unify the different platform errors as much as possible but
this is an ongoing effort. In particular, the Windows Win32 documentation mostly
does not specify what errors a function can throw. As a result, when an error
occurs on Windows process-lib will usually return `PROCESS_LIB_UNKNOWN_ERROR`.
However, process-lib also provides the function `process_system_error` which
gives the user the actual system error. Use this function to retrieve the actual
system error and file an issue with the system error and the process-lib
function that returned it. This way we can incrementally find all remaining
unknown errors and add them to process-lib.

## Gotcha's

- While `process_terminate` allows the child process to perform cleanup it is up
  to the child process to correctly clean up after itself. process-lib only
  sends a termination signal to the child process itself. The child process is
  responsible for cleaning up its own child processes and other resources in its
  signal handler.

- When using `process_kill` the child process does not receive a chance to
  perform cleanup which could result in leaking of resources. Chief among these
  leaks is that the child process will not be able to stop its own child
  processes. Always let a child process exit normally or try to stop it with
  `process_terminate` before switching to `process_kill`.

- (POSIX) On POSIX a parent process is required to wait on a child process
  (using `process_wait`) after it has exited before all resources related to
  that process can be freed by the kernel. If the parent doesn't wait on a child
  process after it exits, the child process becomes a
  [zombie process](https://en.wikipedia.org/wiki/Zombie_process).

- (Windows) `process_kill` is not guaranteed to kill a process on Windows. For
  more information, read the Remarks section in the documentation of the
  [TerminateProcess](<https://msdn.microsoft.com/en-us/library/windows/desktop/ms686714(v=vs.85).aspx>)
  function that process-lib uses to kill processes on Windows.

- (Windows) Immediately terminating a process after starting it on Windows might
  result in an error window with error 0xc0000142 popping up. This indicates the
  process was terminated before it was fully initialized. I was not able to find
  a Windows function that allows waiting until a console process is fully
  initialized. This problem shouldn't pop up with normal use of the library
  since most of the time you'll want to read/write to the process or wait until
  it exits normally.

  If someone runs into this problem, I mitigated it in process-lib's tests by
  waiting a few milliseconds using `process_wait` before terminating the
  process.

- (OSX/Windows) On POSIX systems, file descriptors are inherited by default by
  child processes when calling
  [execve](http://man7.org/linux/man-pages/man2/execve.2.html). To prevent
  leaking file descriptors to child processes, POSIX provides a function `fcntl`
  which can be used to set the `FD_CLOEXEC` flag on the file descriptors
  returned by `pipe` which causes them to get closed on exec. However, using
  `fcntl` introduces a race condition since any process created in another
  thread after calling `pipe` but before calling `fcntl` will still inherit the
  file descriptors returned by `pipe`. To get around this race condition Linux
  and the BSD's provide `pipe2` which takes the `O_CLOEXEC` flag as an argument
  and ensures the file descriptors of the created pipe cannot be inherited by
  child processes. However, `pipe2` is not available on OSX so we fall back to
  `pipe` with `fcntl` on OSX. This means that it is possible for file
  descriptors to leak on OSX if processes are created at the same time in
  multiple threads.

  Windows Vista added the `STARTUPINFOEXW` structure in which we can put a list
  of handles that should be inherited. Only these handles are inherited by the
  child process. Note that this only stops process-lib's processes from
  inheriting unintended handles. Other code in your application that calls
  `CreateProcess` without passing a `STARTUPINFOEXW` struct containing the
  handles it should inherit can still unintentionally inherit handles meant for
  a process-lib process.

## Unsupported Platforms

This list contains the platform version that we know process-lib won't work on.
Note that platforms not on this list might not work either. If you encounter an
issue with a platform not on this list, please open an issue.

- Windows < Vista: `STARTUPINFOEXW` is only available from Vista onwards
- FreeBSD < 10: pipe2 is only available from FreeBSD 10 onwards
- Linux < 2.6: pipe2 is only available from Linux 2.6 onwards

## Design

process-lib is designed to be a minimal wrapper around the platform-specific
API's for starting a process, interacting with its standard streams and finally
terminating it.

### Opaque pointer

process-lib uses a process struct to store information between calls to library
functions. This struct is forward-declared in process.h with each
platform-specific implementation hidden in the source files for that platform.

This struct is typedefed to process_type and exposed to the user as an opaque
pointer (process_type \*).

```c
typedef struct process process_type;
```

The process.h header only contains a forward-declaration of the process struct.
We provide an implementation in the source files of each supported platform
where we store platform-specific members such as pipe handles, process id's,
....

To enable the user to allocate memory for the opaque pointer we provide the
`process_size` function that returns the required size for the opaque pointer on
each platform. This function can be used as follows:

```c
process_type *process = malloc(process_size());
```

Advantages:

- Fewer includes required in process.h

  Because we only have a forward declaration of the process struct in process.h
  we don't need any platform-specific includes (such as windows.h) in the header
  file to define the data types of all the members that the struct contains.

- No leaking of implementation details

  Including the process.h header only gives the user access to the forward
  declaration of the process struct and not its implementation which means its
  impossible to access the internals of the struct outside of the library. This
  allows us to change its implementation between versions without having to
  worry about breaking user code.

Disadvantages:

- No simple allocation on the stack

  Because including process.h does not give the compiler access to the full
  definition of the process struct it is unable to allocate its implementation
  on the stack since it doesn't know its size. Allocating on the stack is still
  possible but requires functions such as
  [alloca](https://linux.die.net/man/3/alloca) which is harder compared to just
  writing

  ```c
  process_type process;
  ```

- Not possible to allocate on the heap without help from the library

  Because the compiler doesn't know the size of the process struct the user
  can't easily allocate the process struct on the heap because it can't figure
  out its size with sizeof.

  We already mentioned that we solve this problem by providing the
  `process_size` function that returns the size of the process struct.

### Memory allocation

process-lib aims to do as few dynamic memory allocations as possible in its own
code (not counting allocations that happen in system calls). As of this moment,
dynamic memory allocation is only done on Windows:

- When converting the array of program arguments to a single string as required
  by the
  [CreateProcess](<https://msdn.microsoft.com/en-us/library/windows/desktop/ms682425(v=vs.85).aspx>)
  function and when converting utf-8 strings to utf-16 (Windows native
  encoding).
- When converting utf-8 strings to utf-16 (Windows only supports utf-16) for
  unicode.

I have not found a way to avoid allocating memory while keeping a cross-platform
api for POSIX and Windows. (Windows `CreateProcessW` requires a single utf-16
string of arguments delimited by spaces while POSIX `execvp` requires an array
of utf-8 string arguments). Since process-lib's api takes process arguments as
an array of utf-8 strings we have to allocate memory to convert the array into a
single utf-16 string as required by Windows.

process-lib uses the standard `malloc` and `free` functions to allocate and free
memory. However, providing support for custom allocators should be
straightforward. If you need them, please open an issue.

### (POSIX) Waiting on child process with timeout

I did not find a counterpart for the Windows
[WaitForSingleObject](<https://msdn.microsoft.com/en-us/library/windows/desktop/ms687032(v=vs.85).aspx>)
function which can be used to wait until a process exits or the provided timeout
expires. POSIX has a similar function called
[waitpid](https://linux.die.net/man/2/waitpid) but this function does not
support specifying a timeout value.

To support waiting with a timeout value on POSIX, each process is put in its own
process group with the same id as the process id with a call to `setpgid` after
forking the process. When calling the `process_wait` function, a timeout process
is forked which we put in the same process group as the process we want to wait
for with the same `setpgid` function and puts itself to sleep for the requested
amount of time (timeout value) before exiting. We then call the `waitpid`
function in the main process but instead of passing the process id of the
process we want to wait for we pass the negative value of the process id
`-process->pid`. Passing a negative value for the process id to `waitpid`
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

process-lib uses a fork-exec model to start new child processes on POSIX
systems. A problem that occured is that process-lib needs to differentiate
between errors that happened before the exec call (which are errors from
process-lib) and errors after the exec call (which are errors from the child
process itself). To do this we create an extra pipe in the parent procces with
the `FD_CLOEXEC` flag set and write any errors before and from exec to that
pipe. If we then read from the error pipe after forking the `read` call will
either read 0 which means exec was called and the write endpoint was closed
(because of the `FD_CLOEXEC` flag) or it reads a single integer (errno) which
indicates an error occured before or during exec.

This solution was inspired by [this](https://stackoverflow.com/a/1586277) Stack
Overflow answer.

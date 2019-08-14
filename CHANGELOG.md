# Changelog

## 9.0.0

## General

- Remove `argc` parameter from `reproc_start` and `process::start`.

  We can trivially calculate `argc` internally in reproc since `argv` is
  required to end with a `NULL` value.

- Improve implementation of `reproc_wait` with a timeout on POSIX systems.

  Instead of spawning a new process to implement the timeout, we now use
  `sigtimedwait` on Linux and `kqueue` on Darwin to wait on `SIGCHLD` signals
  and check if the process we're waiting on has exited after each received
  `SIGCHLD` signal.

- Remove `vfork` usage.

  Clang analyzer was indicating a host of errors in our `vfork` implementation.
  We also discovered tests were behaving differently on macOS depending on
  whether `vfork` was enabled or disabled. As we do not have the expertise to
  verify if `vfork` is working correctly, we opt to remove it.

  As disabling `vfork` impacted the working directory test on MacOS, this change
  will likely break code passing relative paths to the `working_directory`
  parameter of `reproc_start` on MacOS. With `vfork` enabled, the path was
  relative to the parent process working directory. With `vfork` removed, the
  path will be relative to the working directory of the child process (in other
  words: relative to the working directory passed to the `working_directory`
  parameter of `reproc_start`).

  As we already recommend not passing relative paths as the working directory to
  `reproc_start`, we fit this change in a feature release instead of a major
  release.

- Ensure passing a custom working directory and a relative executable path
  behaves consistently on all supported platforms.

  Previously, calling `reproc_start` with a relative executable path combined
  with a custom working directory would behave differently depending on which
  platform the code was executed on. On POSIX systems, the relative executable
  path would be resolved relative to the custom working directory. On Windows,
  the relative executable path would be resolved relative to the parent process
  working directory. Now, relative executable paths are always resolved relative
  to the parent process working directory.

## reproc

- Update `reproc_strerror` to return the actual system error string of the error
  code returned by `reproc_system_error` instead of "system error" when passed
  `REPROC_ERROR_SYSTEM` as argument.

  This should make debugging reproc errors a lot easier.

# reproc++

- Move sinks into `sink` namespace and remove `_sink` suffix from all sinks.

- Add `discard` sink that discards all output read from a stream.

  This is useful when a child process produces a lot of output that we're not
  interested in and cannot handle the output stream being closed or full. When
  this is the case, simply start a thread that drains the stream with a
  `discard` sink.

- Update `process::start` to work with any kind of string type.

  Every string type that implements a `size` method and the index operator can
  now be passed in a container to `process::start`. `working_directory` now
  takes a `const char *` instead of a `std::string *`.

## 8.0.1

- Correctly escape arguments on Windows.

  See [#18](https://github.com/DaanDeMeyer/reproc/issues/18) for more
  information.

## 8.0.0

- Change `reproc_parse` and `reproc_drain` argument order.

  `context` is now the last argument instead of the first.

- Use `uint8_t *` as buffer type instead of `char *` or `void *`

  `uint8_t *` more clearly indicates reproc is working with buffers of bytes
  than `char *` and `void *`. We choose `uint8_t *` over `char *` to avoid
  errors caused by passing data read by reproc directly to functions that expect
  null-terminated strings (data read by reproc is not null-terminated).

## 7.0.0

## General

- Rework error handling.

  Trying to abstract platform-specific errors in `REPROC_ERROR` and
  `reproc::errc` turned out to be harder than expected. On POSIX it remains very
  hard to figure out which errors actually have a chance of happening and
  matching `reproc::errc` values to `std::errc` values is also ambiguous and
  prone to errors. On Windows, there's hardly any documentation on which system
  errors functions can return so 90% of the time we were just returning
  `REPROC_UNKNOWN_ERROR`. Furthermore, many operating system errors will be
  fatal for most users and we suspect they'll all be handled similarly (stopping
  the application or retrying).

  As a result, in this release we stop trying to abstract system errors in
  reproc. All system errors in `REPROC_ERROR` were replaced by a single value
  (`REPROC_ERROR_SYSTEM`). `reproc::errc` was renamed to `reproc::error` and
  turned into an error code instead of an error condition and only contains the
  reproc-specific errors.

  reproc users can still retrieve the specific system error using
  `reproc_system_error`.

  reproc++ users can still match against specific system errors using the
  `std::errc` error condition enum
  (<https://en.cppreference.com/w/cpp/error/errc>) or print a string
  presentation of the error using the `message` method of `std::error_code`.

  All values from `REPROC_ERROR` are now prefixed with `REPROC_ERROR` instead of
  `REPROC` which helps reduce clutter in code completion.

- Azure Pipelines CI now includes Visual Studio 2019.

- Various smaller improvements and fixes.

## CMake

- Introduce `REPROC_MULTITHREADED` to configure whether reproc should link
  against pthreads.

  By default, `REPROC_MULTITHREADED` is enabled to prevent accidental undefined
  behaviour caused by forgetting to enable `REPROC_MULTITHREADED`. Advanced
  users might want to disable `REPROC_MULTITHREADED` when they know for certain
  their code won't use more than a single thread.

- doctest is now downloaded at configure time instead of being vendored inside
  the reproc repository.

  doctest is only downloaded if `REPROC_TEST` is enabled.

## 6.0.0

## General

- Added Azure Pipelines CI.

  Azure Pipelines provides 10 parallel jobs which is more than Travis and
  Appveyor combined. If it turns out to be reliable Appveyor and Travis will
  likely be dropped in the future. For now, all three are enabled.

- Code cleanup and refactoring.

## CMake

- Renamed `REPROC_TESTS` to `REPROC_TEST`.
- Renamed test executable from `tests` to `test`.

## reproc

- Renamed `reproc_type` to `reproc_t`.

  We chose `reproc_type` initially because `_t` belongs to POSIX but we switch
  to using `_t` because `reproc` is a sufficiently unique name that we don't
  have to worry about naming conflicts.

- reproc now keeps track of whether a process has exited and its exit status.

  Keeping track of whether the child process has exited allows us to remove the
  restriction that `reproc_wait`, `reproc_terminate`, `reproc_kill` and
  `reproc_stop` cannot be called again on the same process after completing
  successfully once. Now, if the process has already exited, these methods don't
  do anything and return `REPROC_SUCCESS`.

- Added `reproc_running` to allow checking whether a child process is still
  running.

- Added `reproc_exit_status` to allow querying the exit status of a process
  after it has exited.

- `reproc_wait` and `reproc_stop` lost their `exit_status` output parameter.

  Use `reproc_exit_status` instead to retrieve the exit status.

## reproc++

- Added `process::running` and `process::exit_status`.

  These delegate to `reproc_running` and `reproc_exit_status` respectively.

- `process::wait` and `process::stop` lost their `exit_status` output parameter.

  Use `process::exit_status` instead.

## 5.0.1

### reproc++

- Fixed compilation error caused by defining `reproc::process`'s move assignment
  operator as default in the header which is not allowed when a
  `std::unique_ptr` member of an incomplete type is present.

## 5.0.0

### General

- Added and rewrote implementation documentation.
- General refactoring and simplification of the source code.

### CMake

- Raised minimum CMake version to 3.13.

  Tests are now added to a single target `reproc-tests` in each subdirectory
  included with `add_subdirectory`. Dependencies required to run the added tests
  are added to `reproc-tests` with `target_link_libraries`. Before CMake 3.13,
  `target_link_libraries` could not modify targets created outside of the
  current directory which is why CMake 3.13 is needed.

- `REPROC_CI` was renamed to `REPROC_WARNINGS_AS_ERRORS`.

  This is a side effect of upgrading cddm. The variable was renamed in cddm to
  more clearly indicate its purpose.

- Removed namespace from reproc's targets.

  To link against reproc or reproc++, you now have to link against the target
  without a namespace prefix:

  ```cmake
  find_package(reproc) # or add_subdirectory(external/reproc)
  target_link_libraries(myapp PRIVATE reproc)

  find_package(reproc++) # or add_subdirectory(external/reproc++)
  target_link_libraries(myapp PRIVATE reproc++)
  ```

  This change was made because of a change in cddm (a collection of CMake
  functions to make setting up new projects easier) that removed namespacing and
  aliases of library targets in favor of namespacing within the target name
  itself. This change was made because the original target can still conflict
  with other targets even after adding an alias. This can cause problems when
  using generic names for targets inside the library itself. An example
  clarifies the problem:

  Imagine reproc added a target for working with processes asynchronously. In
  the previous naming scheme, we'd do the following in reproc's CMake build
  files:

  ```cmake
  add_library(async "")
  add_library(reproc::async ALIAS async)
  ```

  However, there's a non-negligible chance that someone using reproc might also
  have a target named async which would result in a conflict when using reproc
  with `add_subdirectory` since there'd be two targets with the same name. With
  the new naming scheme, we'd do the following instead:

  ```cmake
  add_library(reproc-async "")
  ```

  This has almost zero chance of conflicting with user's target names. The
  advantage is that with this scheme we can use common target names without
  conflicting with user's target names which was not the case with the previous
  naming scheme.

### reproc

- Removed undefined behaviour in Windows implementation caused by casting an int
  to an unsigned int.

- Added a note to `reproc_start` docs about the behaviour of using a executable
  path relative to the working directory combined with a custom working
  directory for the child process on different platforms.

- We now retrieve the file descriptor limit in the parent process (using
  `sysconf`) instead of in the child process because `sysconf` is not guaranteed
  to be async-signal-safe which all functions called in a child process after
  forking should be.

- Fixed compilation issue when `ATTRIBUTE_LIST_FOUND` was undefined (#15).

### reproc++

- Generified `process::start` so it works with any container of `std::string`
  satisfying the
  [SequenceContainer](https://en.cppreference.com/w/cpp/named_req/SequenceContainer)
  interface.

## 4.0.0

### General

- Internal improvements and documentation fixes.

### reproc

- Added `reproc_parse` which mimics reproc++'s `process::parse`.
- Added `reproc_drain` which mimics reproc++'s `process::drain` along with an
  example that explains how to use it.

  Because C doesn't support lambda's, both of these functions take a function
  pointer and an extra context argument which is passed to the function pointer
  each time it is called. The context argument can be used to store any data
  needed by the given function pointer.

### reproc++

- Renamed the `process::read` overload which takes a parser to `process::parse`.

  This breaking change was done to keep consistency with reproc where we added
  `reproc_parse`. We couldn't add another `reproc_read` since C doesn't support
  overloading so we made the decision to rename `process::read` to
  `process::parse` instead.

- Changed `process::drain` sinks to return a boolean instead of `void`.

  Before this change, the only way to stop draining a process was to throw an
  exception from the sink. By changing sinks to return `bool`, a sink can tell
  `drain` to stop if an error occurs by returning `false`. The error itself can
  be stored in the sink if needed.

## 3.1.3

### CMake

- Update project version in CMakeLists.txt from 3.0.0 to the actual latest
  version (3.1.3).

## 3.1.2

### pkg-config

- Fix pkg-config install prefix.

## 3.1.0

### CMake

- Added `REPROC_INSTALL_PKGCONFIG` to control whether pkg-config files are
  installed or not (default: `ON`).

  The vcpkg package manager has no need for the pkg-config files so we added an
  option to disable installing them.

- Added `REPROC_INSTALL_CMAKECONFIGDIR` and `REPROC_INSTALL_PKGCONFIGDIR` to
  control where cmake config files and pkg-config files are installed
  respectively (default: `${CMAKE_INSTALL_LIBDIR}/cmake` and
  `${CMAKE_INSTALL_LIBDIR}/pkgconfig`).

  reproc already uses the values from `GNUInstallDirs` when generating its
  install rules which are cache variables that be overridden by users. However,
  `GNUInstallDirs` does not include variables for the installation directories
  of CMake config files and pkg-config files. vcpkg requires cmake config files
  to be installed to a different directory than the directory reproc used until
  now. These options were added to allow vcpkg to control where the config files
  are installed to.

## 3.0.0

### General

- Removed support for Doxygen (and as a result `REPROC_DOCS`).

  All the Doxygen directives made the header docstrings rather hard to read
  directly. Doxygen's output was also too complicated for a simple library such
  as reproc. Finally, Doxygen doesn't really provide any intuitive support for
  documenting a set of libraries. I have an idea for a Doxygen alternative using
  libclang and cmark but I'm not sure when I'll be able to implement it.

### CMake

- Renamed `REPROCXX` option to `REPROC++`.

  `REPROCXX` was initially chosen because CMake didn't recommend using anything
  other than letters and underscores for variable names. However, `REPROC++`
  turns out to work without any problems so we use it since it's the expected
  name for an option to build reproc++.

- Stopped modifying the default `CMAKE_INSTALL_PREFIX` on Windows.

  In 2.0.0, when installing to the default `CMAKE_INSTALL_PREFIX`, you would end
  up with `C:\Program Files (x86)\reproc` and `C:\Program Files (x86)\reproc++`
  when installing reproc. In 3.0.0, the default `CMAKE_INSTALL_PREFIX` isn't
  modified anymore and all libraries are installed to `CMAKE_INSTALL_PREFIX` in
  exactly the same way as they are on UNIX systems (include and lib
  subdirectories directly beneath the installation directory). Sticking to the
  defaults makes it easy to include reproc in various package managers such as
  vcpkg.

### reproc

- `reproc_terminate` and `reproc_kill` don't call `reproc_wait` internally
  anymore. `reproc_stop` has been changed to call `reproc_wait` after calling
  `reproc_terminate` or `reproc_kill` so it still behaves the same.

  Previously, calling `reproc_terminate` on a list of processes would only call
  `reproc_terminate` on the next process after the previous process had exited
  or the timeout had expired. This made terminating multiple processes take
  longer than required. By removing the `reproc_wait` call from
  `reproc_terminate`, users can first call `reproc_terminate` on all processes
  before waiting for each of them with `reproc_wait` which makes terminating
  multiple processes much faster.

- Default to using `vfork` instead of `fork` on POSIX systems.

  This change was made to increase `reproc_start`'s performance when the parent
  process is using a large amount of memory. In these scenario's, `vfork` can be
  a lot faster than `fork`. Care is taken to make sure signal handlers in the
  child don't corrupt the state of the parent process. This change induces an
  extra constraint in that `set*id` functions cannot be called while a call to
  `reproc_start` is in process, but this situation is rare enough that the
  tradeoff for better performance seems worth it.

  A dependency on pthreads had to be added in order to safely use `vfork` (we
  needed access to `pthread_sigmask`). The CMake and pkg-config files have been
  updated to automatically find pthreads so users don't have to find it
  themselves.

- Renamed `reproc_error_to_string` to `reproc_strerror`.

  The C standard library has `strerror` for retrieving a string representation
  of an error. By using the same function name (prefixed with reproc) for a
  function that does the same for reproc's errors, new users will immediately
  know what the function does.

### reproc++

- reproc++ now takes timeouts as `std::chrono::duration` values (more specific
  `reproc::milliseconds`) instead of unsigned ints.

  Taking the `reproc::milliseconds` type explains a lot more about the expected
  argument than taking an unsigned int. C++14 also added chrono literals which
  make constructing `reproc::milliseconds` values a lot more concise
  (`reproc::milliseconds(2000)` => `2000ms`).

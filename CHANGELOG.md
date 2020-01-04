# Changelog

## 10.0.0

### reproc

- Remove `reproc_parse`.

  Instead of checking for `REPROC_EPIPE` (previously
  `REPROC_ERROR_STREAM_CLOSED`), simply check if the given parser has a full
  message available. If it doesn't, the output streams closed unexpectedly.

- Remove `reproc_running` and `reproc_exit_status`.

  When calling `reproc_running`, it would wait with a zero timeout if the
  process was still running and check if the wait timed out. However, a call to
  wait can fail for other reasons as well which were all ignored by
  `reproc_running`. Instead of `reproc_running`, A call to `reproc_wait` with a
  timeout of zero should be used to check if a process is still running.
  `reproc_wait` now also returns the exit status if the process exits or has
  already exited which removes the need for `reproc_exit_status`.

- Read from both stdout and stderr in `reproc_read` to avoid deadlocks and
  indicate which stream `reproc_read` was read from.

  Previously, users would indicate the stream they wanted to read from when
  calling `reproc_read`. However, this lead to issues with programs that write
  to both stdout and stderr as a user wouldn't know whether stdout or stderr
  would have output available to read. Reading from only the stdout stream
  didn't work as the parent could be blocked on reading from stdout while the
  child was simultaneously blocked on writing to stderr leading to a deadlock.
  To get around this, users had to start up a separate thread to read from both
  stdout and stderr at the same time which was a lot of extra work just to get
  the output of external programs that write to both stdout and stderr. Now,
  reproc takes care of avoiding the deadlock by checking which of stdout/stderr
  can be read from, doing the actual read and indicating to the user which
  stream was read from.

  Practically, instead of passing `REPROC_STREAM_OUT` or `REPROC_STREAM_ERR` to
  `reproc_read`, you now pass a pointer to a `REPROC_STREAM` variable instead
  which `reproc_read` will set to `REPROC_STREAM_OUT` or `REPROC_STREAM_ERR`
  depending on which stream it read from.

  If both streams have been closed by the child process, `reproc_read` returns
  `REPROC_EPIPE`.

  Because of the changes to `reproc_read`, `reproc_drain` now also reads from
  both stdout and stderr and indicates the stream that was read from to the
  given sink function via an extra argument passed to the sink.

- Read the output of both stdout and stderr into a single contiguous
  null-terminated string in `reproc_sink_string`.

- Remove the `bytes_written` parameter of `reproc_write`.

  `reproc_write` now always writes `size` bytes to the standard input of the
  child process. Partial writes do not have to be handled by users anymore and
  are instead handled by reproc internally.

- Define `_GNU_SOURCE` and `_WIN32_WINNT` only in the implementation files that
  need them.

  This helps keep track of where we're using functionality that requires extra
  definitions and makes building reproc in all kinds of build systems simpler as
  the compiler invocations to build reproc get smaller as a result.

- Change the error handling in the public API to return negative `errno` (POSIX)
  or `GetLastError` (Windows) style values. `REPROC_ERROR` is replaced by extern
  constants that are assigned the correct error value based on the platform
  reproc is built for. Instead of returning `REPROC_ERROR`, most functions in
  reproc's API now return `int` when they can fail. Because system errors are
  now returned directly, there's no need anymore for `REPROC_ERROR` and
  `reproc_error_system` and they has been removed.

  Error handling before 10.0.0:

  ```c
  REPROC_ERROR error = reproc_start(...);
  if (error) {
    goto cleanup;
  }

  cleanup:
  if (error) {
    fprintf(stderr, "%s", reproc_strerror(error));
  }
  ```

  Error handling from 10.0.0 onwards:

  ```c
  int r = reproc_start(...);
  if (r < 0) {
    goto cleanup;
  }

  cleanup:
  if (r < 0) {
    fprintf(stderr, "%s", reproc_strerror(r));
  }
  ```

- Hide the internals of `reproc_t`.

  Instances of `reproc_t` are now allocated on the heap by calling `reproc_new`.
  `reproc_destroy` releases the memory allocated by `reproc_new`.

- Take optional arguments via the `reproc_options` struct in `reproc_start`.

  When using designated initializers, calls to `reproc_start` are now much more
  readable than before. Using a struct also makes it much easier to set all
  options to their default values (`reproc_options options = { 0 };`). Finally,
  we can add more options in further releases without requiring existing users
  to change their code.

- Support redirecting the child process standard streams to `/dev/null` (POSIX)
  or `NUL` (Windows) in `reproc_start` via the `redirect` field in
  `reproc_options`.

  This is especially useful when you're not interested in the output of a child
  process as redirecting to `/dev/null` doesn't require regularly flushing the
  output pipes of the process to prevent deadlocks as is the case when
  redirecting to pipes.

- Support redirecting the child process standard streams to the parent process
  standard streams in `reproc_starŧ` via the `redirect` field in
  `reproc_options`.

  This is useful when you want to interleave child process output with the
  parent process output.

- Modify `reproc_start` and `reproc_destroy` to work like the reproc++ `process`
  class constructor and destructor.

  The `stop_actions` field in `reproc_options` can be used to define up to three
  stop actions that are executed when `reproc_destroy` is called if the child
  process is still running. If no explicit stop actions are given,
  `reproc_destroy` defaults to waiting indefinitely for the child process to
  exit.

- Return the amount of bytes read from `reproc_read` if it succeeds.

  This is made possible by the new error handling scheme. Because errors are all
  negative values, we can use the positive range of an `int` as the normal
  return value if no errors occur.

- Return the exit status from `reproc_wait` and `reproc_stop` if they succeed.

  Same reasoning as above. If the child process has already exited,
  `reproc_wait` and `reproc_stop` simply returns the exit status again.

- Do nothing when `NULL` is passed to `reproc_destroy` and always return `NULL`
  from `reproc_destroy`.

  This allows `reproc_destroy` to be safely called on the same instance multiple
  times when assigning the result of `reproc_destroy` to the same instance
  (`process = reproc_destroy(process)`).

- Take stop actions via the `reproc_stop_actions` struct in `reproc_stop`.

  This makes it easier to store stop action configurations both in and outside
  of reproc.

- Add 256 to signal exit codes returned by `reproc_wait` and `reproc_stop`.

  This prevents conflicts with normal exit codes.

- Add `REPROC_SIGTERM` and `REPROC_SIGKILL` constants to match against signal
  exit codes.

  These also work on Windows and correspond to the exit codes returned by
  sending the `CTRL-BREAK` signal and calling `TerminateProcess` respectively.

- Rename `REPROC_CLEANUP` to `REPROC_STOP`.

  Naming the enum after the function it is passed to (`reproc_stop`) is simpler
  than using a different name.

- Inline the `reproc_sink_string` and `reproc_sink_discard` implementations in
  the sink.h header.

  This avoids issues with allocating across module (DLL) boundaries on Windows.

- Rewrite tests in C using CTest and `assert` and remove doctest.

  Doctest is a great library but we don't really lose anything major by
  replacing it with CTest and asserts. On the other hand, we lose a dependency,
  don't need to download stuff from CMake anymore and tests compile
  significantly faster.

- Return `REPROC_EINVAL` from public API functions when passed invalid
  arguments.

- Make `reproc_strerror` thread-safe.

- Make `reproc_drain` take a separate sink for each output stream. Sinks are now
  passed via the `reproc_sink` type.

  Using separate sinks for both output streams allows for a lot more
  flexibility. To use a single sink for both output streams, simply pass the
  same sink to both the `out` and `err` arguments of `reproc_drain`.

- Support passing timeouts to `reproc_read`, `reproc_write` and `reproc_drain`.

  Pass `REPROC_INFINITE` as the timeout to retain the old behaviour.

- Use `int` to represent timeout values.

### reproc++

- Remove `process::parse`, `process::exit_status` and `process::running`.

  Consequence of the equivalents in reproc being removed.

- Take separate `out` and `err` arguments in the `sink::string` and
  `sink::ostream` constructors that receive output from the stdout and stderr
  streams of the child process respectively.

  To combine the output from the stdout and stderr streams, simply pass the same
  `string` or `ostream` to both the `out` and `err` arguments.

- Modify `process::read` to return a tuple of the stream read from, the amount
  of bytes read and an error code. The stream read from and amount of bytes read
  are only valid if `process::read` succeeds.

  `std::tie` can be used pre-C++17 to assign the tuple's contents to separate
  variables.

- Modify `process::wait` and `process::stop` to return a pair of exit status and
  error code. The exit status is only valid if `process::wait` or
  `process::stop` succeeds.

- Alias `reproc::error` to `std::errc`.

  As OS errors are now used everywhere, we can simply use `std::errc` for all
  error handling instead of defining our own error code.

- Add `signal::terminate` and `signal::kill` constants.

  These are aliases for `REPROC_SIGTERM` and `REPROC_SIGKILL` respectively.

- Inline all sink implementations in sink.hpp.

- Add `sink::thread_safe::string` which is a thread-safe version of
  `sink::string`.

- Make `process::drain` take a separate sink for each output stream.

  Same reasoning as `reproc_drain`.

- Modify all included sinks to support the new `process::drain` behaviour.

- Support passing timeouts to `process::read`, `process::write` and
  `process::drain`.

  The methods default to waiting indefinitely which matches their old behaviour.

### CMake

- Drop required CMake version to CMake 3.12.
- Add CMake 3.16 as a supported CMake version.
- Build reproc++ with `-pthread` when `REPROC_MULTITHREADED` is enabled.

  See https://github.com/DaanDeMeyer/reproc/issues/24 for more information.

- Add `REPROC_WARNINGS` option (default: `OFF`) to build with compiler warnings.
- Add `REPROC_DEVELOP` option (default: `OFF`) which enables a lot of options to
  simplify developing reproc.

  By default, most of reproc's CMake options are disabled to make including
  reproc in other projects as simple as possible. However, when working on
  reproc, we usually wants most options enabled instead. To make enabling all
  options simpler, `REPROC_DEVELOP` was added from which most other options take
  their default value. As a result, enabling `REPROC_DEVELOP` enables all
  options related to developing reproc. Additionally, `REPROC_DEVELOP` takes its
  initial value from an environment variable of the same name so it can be set
  once and always take effect whenever running CMake on reproc's source tree.

- Add `REPROC_OBJECT_LIBRARIES` option to build CMake object libraries.

  In CMake, linking a library against a static library doesn't actually copy the
  object files from the static library into the library. Instead, both static
  libraries have to be installed and depended on by the final executable. By
  using CMake object libraries, the object files are copied into the depending
  static library and no extra artifacts are produced.

- Enable `REPROC_INSTALL` by default unless `REPROC_OBJECT_LIBRARIES` is
  enabled.

  As `REPROC_OBJECT_LIBRARIES` can now be used to depend on reproc without
  generating extra artifacts, we assume that users not using
  `REPROC_OBJECT_LIBRARIES` will want to install the produced artifacts.

- Rename reproc++ to reprocxx inside the CMake build files.

  This was done to allow using reproc as a Meson subproject. Meson doesn't
  accept the '+' character in target names so we use 'x' instead.

- Modify the export headers so that the only extra define necessary is
  `REPROC_SHARED` when using reproc as a shared library on Windows.

  Naturally, this define is added as a CMake usage requirement and doesn't have
  to be worried about when using reproc via `add_subdirectory` or
  `find_package`.

## 9.0.0

### General

- Drop support for Windows XP.

- Add support for custom environments.

  `reproc_start` and `process::start` now take an extra `environment` parameter
  that allows specifying custom environments.

  **IMPORTANT**: The `environment` parameter was inserted before the
  `working_directory` parameter so make sure to update existing usages of
  `reproc_start` and `process::start` so that the `environment` and
  `working_directory` arguments are specified in the correct order.

  To keep the previous behaviour, pass `nullptr` as the environment to
  `reproc_start`/`process::start` or use the `process::start` overload without
  the `environment` parameter.

- Remove `argc` parameter from `reproc_start` and `process::start`.

  We can trivially calculate `argc` internally in reproc since `argv` is
  required to end with a `NULL` value.

- Improve implementation of `reproc_wait` with a timeout on POSIX systems.

  Instead of spawning a new process to implement the timeout, we now use
  `sigtimedwait` on Linux and `kqueue` on macOS to wait on `SIGCHLD` signals and
  check if the process we're waiting on has exited after each received `SIGCHLD`
  signal.

- Remove `vfork` usage.

  Clang analyzer was indicating a host of errors in our usage of `vfork`. We
  also discovered tests were behaving differently on macOS depending on whether
  `vfork` was enabled or disabled. As we do not have the expertise to verify if
  `vfork` is working correctly, we opt to remove it.

- Ensure passing a custom working directory and a relative executable path
  behaves consistently on all supported platforms.

  Previously, calling `reproc_start` with a relative executable path combined
  with a custom working directory would behave differently depending on which
  platform the code was executed on. On POSIX systems, the relative executable
  path would be resolved relative to the custom working directory. On Windows,
  the relative executable path would be resolved relative to the parent process
  working directory. Now, relative executable paths are always resolved relative
  to the parent process working directory.

- Reimplement `reproc_drain`/`process::drain` in terms of
  `reproc_parse`/`process::parse`.

  Like `reproc_parse` and `process::parse`, `reproc_drain` and `process::drain`
  are now guaranteed to always be called once with an empty buffer before
  reading any actual data.

  We now also guarantee that the initial empty buffer is not `NULL` or `nullptr`
  so the received data and size can always be safely passed to `memcpy`.

- Add MinGW support.

  MinGW CI builds were also added to prevent regressions in MinGW support.

### reproc

- Update `reproc_strerror` to return the actual system error string of the error
  code returned by `reproc_system_error` instead of "system error" when passed
  `REPROC_ERROR_SYSTEM` as argument.

  This should make debugging reproc errors a lot easier.

- Add `reproc_sink_string` in `sink.h`, a sink that stores all process output in
  a single null-terminated C string.

- Add `reproc_sink_discard` in `sink.h`, a sink that discards all process
  output.

### reproc++

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

- Fix compilation error when using `process::parse`.

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

### General

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

### CMake

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

### General

- Added Azure Pipelines CI.

  Azure Pipelines provides 10 parallel jobs which is more than Travis and
  Appveyor combined. If it turns out to be reliable Appveyor and Travis will
  likely be dropped in the future. For now, all three are enabled.

- Code cleanup and refactoring.

### CMake

- Renamed `REPROC_TESTS` to `REPROC_TEST`.
- Renamed test executable from `tests` to `test`.

### reproc

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

### reproc++

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

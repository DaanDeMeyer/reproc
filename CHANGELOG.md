# Changelog

## 3.0.0

### General

- Removed support for Doxygen (and as a result `REPROC_DOCS`).

All the Doxygen directives made the header docstrings were pretty hard to read
directly. Doxygen's output also wasn't completely to my liking. I have an idea
for a Doxygen alternative using libclang and cmark but I'm not sure when I'll be
able to implement it.

### CMake

- Renamed `REPROCXX` option to `REPROC++`.

`REPROCXX` was initially chosen because CMake didn't recommend using anything
other than letters and underscores for variable names. However, `REPROC++` turns
out to work without any problems so we use it since it's the expected name for
an option to build reproc++.

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

  This change was made to increase performance `reproc_start`'s performance when
  the parent process uses a large amount of memory. In these scenario's, using
  `vfork` can be a lot faster compared to using `fork`. Care is taken to make
  sure signal handlers in the child don't corrupt the parent process state. This
  change induces an extra constraint in that `set*id` functions cannot be called
  while a call to `reproc_start` is in process, but this situation is rare
  enough that the tradeoff for better performance seems worth it.

  A dependency on pthreads had to be added in order to safely use `vfork` (we
  needed access to `pthread_sigmask`).

### reproc++

- reproc++ now takes timeouts as `std::chrono::duration` values (more specific
  `reproc::milliseconds`) instead of unsigned ints.

  Taking the `reproc::milliseconds` type explains a lot more about the expected
  argument than taking an unsigned int. C++14 also added chrono literals which
  make constructing `reproc::milliseconds` values a lot more concise
  (`reproc::milliseconds(2000)` => `2000ms`).

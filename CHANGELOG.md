# Changelog

## 3.0.0

- reproc++ now takes timeouts as `std::chrono::duration` values (more specific
  `reproc::milliseconds`) instead of unsigned ints.
- `reproc_terminate` and `reproc_kill` don't call `reproc_wait` internally
  anymore. `reproc_stop` has been changed to call `reproc_wait` after calling
  `reproc_terminate` or `reproc_kill` so it still behaves the same.
- Default to using `vfork` instead of `fork` on POSIX systems to increase
  performance when the parent process uses a large amount of memory.

  A dependency on pthreads had to be added in order to safely use `vfork`.



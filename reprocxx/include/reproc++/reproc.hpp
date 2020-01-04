#pragma once

#include <reproc++/arguments.hpp>
#include <reproc++/environment.hpp>
#include <reproc++/export.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <system_error>
#include <tuple>

// Forward declare `reproc_t` so we don't have to include reproc.h in the
// header.
struct reproc_t;

/*! The `reproc` namespace wraps all reproc++ declarations. `process` wraps
reproc's API inside a C++ class. To avoid exposing reproc's API when using
reproc++ all structs, enums and constants of reproc have a replacement in
reproc++. Only differences in behaviour compared to reproc are documented. Refer
to reproc.h and the examples for general information on how to use reproc. */
namespace reproc {

/*! Conversion from reproc `errno` constants to `std::errc` constants:
https://en.cppreference.com/w/cpp/error/errc */
using error = std::errc;

namespace signal {

REPROCXX_EXPORT extern const int kill;
REPROCXX_EXPORT extern const int terminate;

}

/*! Timeout values are passed as `reproc::milliseconds` instead of `int` in
reproc++. */
using milliseconds = std::chrono::duration<int, std::milli>;

enum class redirect { pipe, inherit, discard };

enum class stop { noop, wait, terminate, kill };

struct stop_action {
  stop action;
  milliseconds timeout;
};

struct stop_actions {
  stop_action first;
  stop_action second;
  stop_action third;
};

struct options {
  /*! Implicitly converts from any STL container of string pairs to the
  environment format expected by `reproc_start`. */
  class environment environment;
  const char *working_directory = nullptr;

  struct {
    redirect in;
    redirect out;
    redirect err;
  } redirect = {};

  struct stop_actions stop_actions = {};
};

enum class stream { in, out, err };

REPROCXX_EXPORT extern const milliseconds infinite;

/*! Improves on reproc's API by adding RAII and changing the API of some
functions to be more idiomatic C++. */
class process {

public:
  REPROCXX_EXPORT process();
  REPROCXX_EXPORT ~process() noexcept;

  // Enforce unique ownership of child processes.
  REPROCXX_EXPORT process(process &&other) noexcept;
  REPROCXX_EXPORT process &operator=(process &&other) noexcept;

  /*! `reproc_start` but implicitly converts from STL containers to the
  arguments format expected by `reproc_start`. */
  REPROCXX_EXPORT std::error_code start(const arguments &arguments,
                                        const options &options = {}) noexcept;

  /*! `reproc_read` but returns a tuple of (stream, bytes read, error) and
  defaults to waiting indefinitely for each read to complete.*/
  REPROCXX_EXPORT std::tuple<stream, size_t, std::error_code>
  read(uint8_t *buffer,
       size_t size,
       reproc::milliseconds timeout = reproc::infinite) noexcept;

  /*!
  `reproc_drain` but takes a lambda as its argument instead of a function and
  context pointer. Defaults to waiting indefinitely for each read to complete.

  Unlike `reproc_drain`, it is not possible to pass `NULL` sinks to this method.
  Instead, use `sink::discard` which has the same effect.

  `sink` expects the following signature:

  ```c++
  bool sink(stream stream, const uint8_t *buffer, size_t size);
  ```
  */
  template <typename Sink>
  std::error_code drain(Sink &&out,
                        Sink &&err,
                        reproc::milliseconds timeout = reproc::infinite);

  /*! reproc_write` but defaults to waiting indefinitely for each write to
  complete. */
  REPROCXX_EXPORT std::error_code
  write(const uint8_t *buffer,
        size_t size,
        reproc::milliseconds timeout = reproc::infinite) noexcept;

  REPROCXX_EXPORT std::error_code close(stream stream) noexcept;

  /*! `reproc_wait` but returns a pair of (status, error). */
  REPROCXX_EXPORT std::pair<int, std::error_code>
  wait(milliseconds timeout) noexcept;

  REPROCXX_EXPORT std::error_code terminate() noexcept;

  REPROCXX_EXPORT std::error_code kill() noexcept;

  /*! `reproc_stop` but returns a pair of (status, error). */
  REPROCXX_EXPORT std::pair<int, std::error_code>
  stop(stop_actions stop_actions) noexcept;

private:
  std::unique_ptr<reproc_t, void (*)(reproc_t *)> process_;
};

template <typename Sink>
std::error_code
process::drain(Sink &&out, Sink &&err, reproc::milliseconds timeout)
{
  static constexpr uint8_t initial = 0;

  // A single call to `read` might contain multiple messages. By always calling
  // both sinks once with no data before reading, we give them the chance to
  // process all previous output before reading from the child process again.
  if (!out(stream::in, &initial, 0) || !err(stream::in, &initial, 0)) {
    return {};
  }

  std::array<uint8_t, 4096> buffer = {};
  std::error_code ec;

  while (true) {
    stream stream = {};
    size_t bytes_read = 0;
    std::tie(stream, bytes_read, ec) = read(buffer.data(), buffer.size(),
                                            timeout);
    if (ec) {
      break;
    }

    auto &sink = stream == stream::out ? out : err;

    // `sink` returns false to tell us to stop reading.
    if (!sink(stream, buffer.data(), bytes_read)) {
      break;
    }
  }

  return ec == error::broken_pipe ? std::error_code() : ec;
}

}

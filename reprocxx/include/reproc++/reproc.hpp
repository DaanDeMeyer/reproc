#pragma once

#include <reproc++/arguments.hpp>
#include <reproc++/environment.hpp>
#include <reproc++/export.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <system_error>
#include <tuple>

// Forward declare `reproc_t` so we don't have to include reproc.h in the
// header.
struct reproc_t;

/*! The `reproc` namespace wraps all reproc++ declarations. `process` wraps
reproc's API inside a C++ class. `error` improves on `REPROC_ERROR` by
integrating with C++'s `std::error_code` error handling mechanism. To avoid
exposing reproc's API when using reproc++ all the other structs, enums and
constants of reproc have a replacement in reproc++ as well. */
namespace reproc {

namespace error {

/*! `REPROC_ERROR_WAIT_TIMEOUT` */
constexpr std::errc wait_timeout = std::errc::resource_unavailable_try_again;

/*! `REPROC_ERROR_STREAM_CLOSED` */
#if defined(_WIN32)
// https://github.com/microsoft/STL/pull/406
static const std::error_code stream_closed = { 109, std::system_category() };
#else
constexpr std::errc stream_closed = std::errc::broken_pipe;
#endif

} // namespace error

using milliseconds = std::chrono::duration<unsigned int, std::milli>;

/*! `REPROC_REDIRECT` */
enum class redirect { pipe, inherit, discard };

/*! `REPROC_STOP` */
enum class stop { noop, wait, terminate, kill };

/*! `REPROC_STOP_ACTION` */
struct stop_action {
  stop action;
  milliseconds timeout;
};

/*! `REPROC_STOP_ACTIONS` */
struct stop_actions {
  stop_action first;
  stop_action second;
  stop_action third;
};

/*! `reproc_options` */
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

/*! `REPROC_STREAM` */
enum class stream { in, out, err };

/*! `REPROC_INFINITE` */
REPROCXX_EXPORT extern const milliseconds infinite;

/*! Improves on reproc's API by adding RAII and changing the API of some
functions to be more idiomatic C++. */
class process {

public:
  /*! `reproc_new` */
  REPROCXX_EXPORT process();

  /*! `reproc_destroy` */
  REPROCXX_EXPORT ~process() noexcept;

  // Enforce unique ownership of child processes.
  REPROCXX_EXPORT process(process &&other) noexcept;
  REPROCXX_EXPORT process &operator=(process &&other) noexcept;

  /*! `reproc_start` but implicitly converts from STL containers to the
  arguments format expected by `reproc_start`. */
  REPROCXX_EXPORT std::error_code start(const arguments &arguments,
                                        const options &options = {}) noexcept;

  /*! `reproc_read` but returns a tuple of (stream read from, amount of bytes
  read, error_code) instead of taking output arguments by pointer. */
  REPROCXX_EXPORT std::tuple<stream, size_t, std::error_code>
  read(uint8_t *buffer, size_t size) noexcept;

  /*!
  `reproc_drain` but takes a lambda as its argument instead of a function and
  context pointer.

  `sink` expects the following signature:

  ```c++
  bool sink(stream stream, const uint8_t *buffer, size_t size);
  ```
  */
  template <typename Sink>
  std::error_code drain(Sink &&sink);

  /*! `reproc_write` */
  REPROCXX_EXPORT std::error_code write(const uint8_t *buffer,
                                        size_t size) noexcept;

  /*! `reproc_close` */
  REPROCXX_EXPORT void close(stream stream) noexcept;

  /*! `reproc_running` */
  REPROCXX_EXPORT bool running() noexcept;

  /*! `reproc_wait` */
  REPROCXX_EXPORT std::error_code wait(milliseconds timeout) noexcept;

  /*! `reproc_terminate` */
  REPROCXX_EXPORT std::error_code terminate() noexcept;

  /*! `reproc_kill` */
  REPROCXX_EXPORT std::error_code kill() noexcept;

  /*! `reproc_stop` */
  REPROCXX_EXPORT std::error_code stop(stop_actions stop_actions) noexcept;

  /*! `reproc_exit_status` */
  REPROCXX_EXPORT int exit_status() noexcept;

private:
  std::unique_ptr<reproc_t, void (*)(reproc_t *)> process_;
};

template <typename Sink>
std::error_code process::drain(Sink &&sink)
{
  // We can't use compound literals in C++ to pass the initial value to `sink`
  // so we use a constexpr value instead.
  static constexpr uint8_t initial = 0;

  // A single call to `read` might contain multiple messages. By always calling
  // `sink` once with no data before reading, we give it the chance to process
  // all previous output before reading from the child process again.
  if (!sink(stream::in, &initial, 0)) {
    return {};
  }

  uint8_t buffer[4096]; // NOLINT
  std::error_code ec;

  while (true) {
    stream stream = {};
    size_t bytes_read = 0;
    std::tie(stream, bytes_read, ec) = read(buffer, sizeof(buffer));
    if (ec) {
      break;
    }

    // `sink` returns false to tell us to stop reading.
    if (!sink(stream, buffer, bytes_read)) {
      break;
    }
  }

  return ec == error::stream_closed ? std::error_code() : ec;
}

} // namespace reproc

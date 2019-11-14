#pragma once

#include <reproc++/arguments.hpp>
#include <reproc++/environment.hpp>
#include <reproc++/error.hpp>
#include <reproc++/export.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

// Forward declare `reproc_t` so we don't have to include reproc.h in the
// header.
struct reproc_t;

/*! The `reproc` namespace wraps all reproc++ declarations. `process` wraps
reproc's API inside a C++ class. `error` improves on `REPROC_ERROR` by
integrating with C++'s `std::error_code` error handling mechanism. To avoid
exposing reproc's API when using reproc++ all the other structs, enums and
constants of reproc have a replacement in reproc++ as well. */
namespace reproc {

/*! `REPROC_REDIRECT` */
enum class redirect { pipe, inherit, discard };

/*! `reproc_options` */
struct options {
  class environment environment;
  const char *working_directory = nullptr;

  struct {
    redirect in = redirect::pipe;
    redirect out = redirect::pipe;
    redirect err = redirect::pipe;
  } redirect;
};

/*! `REPROC_STREAM` */
enum class stream { in, out, err };

using milliseconds = std::chrono::duration<unsigned int, std::milli>;
/*! `REPROC_INFINITE` */
REPROCXX_EXPORT extern const milliseconds infinite;

/*! `REPROC_CLEANUP` */
enum class cleanup { noop, wait, terminate, kill };

/*! Improves on reproc's API by wrapping it in a class. Aside from methods that
mimick reproc's API it also adds configurable RAII and several methods that
reduce the boilerplate required to use reproc from idiomatic C++ code. */
class process {

public:
  /*!
  Allocates memory for reproc's `reproc_t` struct.

  The arguments are passed to `stop` in the destructor if the process is still
  running by then.

  Example:

  ```c++
  process example(cleanup::wait, 10000, cleanup::terminate, 5000);
  ```

  If the child process is still running when example's destructor is called, it
  will first wait ten seconds for the child process to exit on its own before
  sending `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waiting five more
  seconds for the child process to exit.

  The default arguments instruct the destructor to wait indefinitely for the
  child process to exit.
  */
  REPROCXX_EXPORT explicit process(cleanup c1 = cleanup::wait,
                                   milliseconds t1 = infinite,
                                   cleanup c2 = cleanup::noop,
                                   milliseconds t2 = milliseconds(0),
                                   cleanup c3 = cleanup::noop,
                                   milliseconds t3 = milliseconds(0));

  /*! Calls `stop` with the arguments provided in the constructor if the child
  process is still running and frees all allocated resources. */
  REPROCXX_EXPORT ~process() noexcept;

  // Enforce unique ownership of child processes.
  REPROCXX_EXPORT process(process &&other) noexcept;
  REPROCXX_EXPORT process &operator=(process &&other) noexcept;

  /*! `reproc_start` */
  REPROCXX_EXPORT std::error_code start(const arguments &arguments,
                                        const options &options = {}) noexcept;

  /*! `reproc_read` */
  REPROCXX_EXPORT std::error_code read(stream *stream,
                                       uint8_t *buffer,
                                       unsigned int size,
                                       unsigned int *bytes_read) noexcept;

  /*!
  Calls `read` on `stream` until `parser` returns false or an error occurs.
  `parser` receives the output after each read.

  `parser` is always called once with an empty buffer to give the parser the
  chance to process all output from the previous call to `parse` one by one.

  `Parser` expects the following signature:

  ```c++
  bool parser(stream stream, const uint8_t *buffer, unsigned int size);
  ```
  */
  template <typename Parser>
  std::error_code parse(Parser &&parser);

  /*!
  `parse` but `stream_closed` is not treated as an error. `Sink` expects the
  same signature as `Parser` in `parse`.

  For examples of sinks, see `sink.hpp`.
  */
  template <typename Sink>
  std::error_code drain(Sink &&sink);

  /*! `reproc_write` */
  REPROCXX_EXPORT std::error_code write(const uint8_t *buffer,
                                        unsigned int size) noexcept;

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
  REPROCXX_EXPORT std::error_code
  stop(cleanup c1,
       milliseconds t1,
       cleanup c2 = cleanup::noop,
       milliseconds t2 = milliseconds(0),
       cleanup c3 = cleanup::noop,
       milliseconds t3 = milliseconds(0)) noexcept;

  /*! `reproc_exit_status` */
  REPROCXX_EXPORT unsigned int exit_status() noexcept;

private:
  std::unique_ptr<reproc_t, void (*)(reproc_t *)> process_;

  cleanup c1_;
  milliseconds t1_;
  cleanup c2_;
  milliseconds t2_;
  cleanup c3_;
  milliseconds t3_;

  static constexpr unsigned int BUFFER_SIZE = 4096;
};

template <typename Parser>
std::error_code process::parse(Parser &&parser)
{
  // We can't use compound literals in C++ to pass the initial value to `parser`
  // so we use a constexpr value instead.
  static constexpr uint8_t initial = 0;

  // A single call to `read` might contain multiple messages. By always calling
  // `parser` once with no data before reading, we give it the chance to process
  // all previous output one by one before reading from the child process again.
  if (!parser(stream::in, &initial, 0)) {
    return {};
  }

  uint8_t buffer[BUFFER_SIZE]; // NOLINT
  std::error_code ec;

  while (true) {
    stream stream = {};
    unsigned int bytes_read = 0;
    ec = read(&stream, buffer, BUFFER_SIZE, &bytes_read);
    if (ec) {
      break;
    }

    // `parser` returns false to tell us to stop reading.
    if (!parser(stream, buffer, bytes_read)) {
      break;
    }
  }

  return ec;
}

template <typename Sink>
std::error_code process::drain(Sink &&sink)
{
  std::error_code ec = parse(std::forward<Sink>(sink));

  return ec == error::stream_closed ? error::success : ec;
}

} // namespace reproc

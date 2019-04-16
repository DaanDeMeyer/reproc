#pragma once

#include <reproc++/error.hpp>
#include <reproc++/export.hpp>

#include <chrono>
#include <memory>
#include <string>

// Forward declare `reproc_type` so we don't have to include reproc.h in the
// header.
struct reproc_type;

/*! The `reproc` namespace wraps all reproc++ declarations. reproc::process
wraps reproc's API inside a C++ class. `reproc::errc` improves on `REPROC_ERROR`
by integrating with C++'s `std::error_code` error handling mechanism. To avoid
exposing reproc's API when using reproc++ all the other enums and constants of
reproc have a replacement in reproc++ as well. */
namespace reproc
{

/*! See `REPROC_STREAM` */
enum class stream {
  /*! `REPROC_IN` */
  in = 0,
  /*! `REPROC_OUT` */
  out = 1,
  /*! `REPROC_ERR` */
  err = 2
};

using milliseconds = std::chrono::duration<unsigned int, std::milli>;
/*! See `REPROC_INFINITE` */
REPROCXX_EXPORT extern const reproc::milliseconds infinite;

/*! See `process::stop` */
enum cleanup {
  /*! Do nothing (no operation). */
  noop = 0,
  /*! `process::wait` */
  wait = 1,
  /*! `process::terminate` */
  terminate = 2,
  /*! `process::kill` */
  kill = 3
};

/*! Improves on reproc's API by wrapping it in a class. Aside from methods that
mimick reproc's API it also adds configurable RAII and several methods that
reduce the boilerplate required to use reproc from idiomatic C++ code. */
class process
{

public:
  /*!
  Allocates memory for reproc's `reproc_type` struct.

  The arguments are passed to `stop` in the destructor if the process is still
  running by then.

  Example:

  ```c++
  reproc::process example(reproc::wait, 10000, reproc::terminate, 5000);
  ```

  If the child process is still running when example's destructor is called, it
  will first wait ten seconds for the child process to exit on its own before
  sending `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waiting five more
  seconds for the child process to exit.

  The default arguments instruct the destructor to wait indefinitely for the
  child process to exit.
  */
  REPROCXX_EXPORT explicit process(cleanup c1 = reproc::wait,
                                   reproc::milliseconds t1 = reproc::infinite);

  REPROCXX_EXPORT process(cleanup c1, reproc::milliseconds t1, cleanup c2,
                          reproc::milliseconds t2);

  REPROCXX_EXPORT process(cleanup c1, reproc::milliseconds t1, cleanup c2,
                          reproc::milliseconds t2, cleanup c3,
                          reproc::milliseconds t3);

  /*! Calls `stop` with the arguments provided in the constructor if the child
  process is still running and frees all allocated resources. */
  REPROCXX_EXPORT ~process() noexcept;

  // Enforce unique ownership of child processes.

  process(const process &) = delete;
  process &operator=(const process &) = delete;

  REPROCXX_EXPORT process(process &&) noexcept;
  REPROCXX_EXPORT process &operator=(process &&) noexcept;

  /*! `reproc_start` */
  REPROCXX_EXPORT std::error_code
  start(int argc, const char *const *argv,
        const char *working_directory = nullptr) noexcept;

  /*!
  Overload of `start` for convenient usage with C++ containers of `std::string`.

  `args` has the same restrictions as `argv` in `reproc_start` except that it
  should not end with `NULL` (`start` allocates a new array which includes the
  missing `NULL` value).

  `working_directory` specifies the working directory. It is optional and
  defaults to `nullptr`.
  */
  template <typename SequenceContainer>
  std::error_code start(const SequenceContainer &args,
                        const std::string *working_directory = nullptr)
  {
    using value_type = typename SequenceContainer::value_type;
    using size_type = typename SequenceContainer::size_type;

    static_assert(std::is_same<value_type, std::string>::value,
                  "Container value_type must be std::string");

    // Turn `args` into an array of C strings.
    auto argv = new const char *[args.size() + 1]; // NOLINT
    for (size_type i = 0; i < args.size(); i++) {
      argv[i] = args[i].c_str();
    }
    argv[args.size()] = nullptr;

    // We assume that `args`'s size fits into an integer.
    auto argc = static_cast<int>(args.size());

    // `std::string *` => `const char *`
    const char *child_working_directory = working_directory != nullptr
                                              ? working_directory->c_str()
                                              : nullptr;

    std::error_code ec = start(argc, argv, child_working_directory);

    delete[] argv; // NOLINT

    return ec;
  }

  /*! `reproc_read` */
  REPROCXX_EXPORT std::error_code read(reproc::stream stream, void *buffer,
                                       unsigned int size,
                                       unsigned int *bytes_read) noexcept;

  /*!
  Calls `read` on `stream` until `parser` returns false or an error occurs.
  `parser` receives the output after each read.

  `parser` is always called once with the empty string to give the parser the
  chance to process all output from the previous call to `parse` one by one.

  `Parser` expects the following signature:

  ```c++
  bool parser(const char *buffer, unsigned int size);
  ```
  */
  template <typename Parser>
  std::error_code parse(reproc::stream stream, Parser &&parser);

  /*!
  Calls `read` on `stream` until it is closed, `sink` returns false or an error
  occurs. `sink` receives the output after each read.

  Note that this method does not report `stream` being closed as an error. This
  is also the main difference with `parse`.

  `Sink` expects the following signature:

  ```c++
  bool sink(const char *buffer, unsigned int size);
  ```

  For examples of sinks, see `sink.hpp`
  */
  template <typename Sink>
  std::error_code drain(reproc::stream stream, Sink &&sink);

  /*! `reproc_write` */
  REPROCXX_EXPORT std::error_code write(const void *buffer,
                                        unsigned int to_write,
                                        unsigned int *bytes_written) noexcept;

  /*! `reproc_close` */
  REPROCXX_EXPORT void close(reproc::stream stream) noexcept;

  /*! `reproc_wait` */
  REPROCXX_EXPORT std::error_code wait(reproc::milliseconds timeout,
                                       unsigned int *exit_status) noexcept;

  /*! `reproc_terminate` */
  REPROCXX_EXPORT std::error_code terminate() noexcept;

  /*! `reproc_kill` */
  REPROCXX_EXPORT std::error_code kill() noexcept;

  /*! `reproc_stop` */
  REPROCXX_EXPORT std::error_code stop(cleanup c1, reproc::milliseconds t1,
                                       cleanup c2, reproc::milliseconds t2,
                                       cleanup c3, reproc::milliseconds t3,
                                       unsigned int *exit_status) noexcept;

  /*! Overload of `stop` with `c3` set to `noop` and `t3` set to 0. */
  REPROCXX_EXPORT std::error_code stop(cleanup c1, reproc::milliseconds t1,
                                       cleanup c2, reproc::milliseconds t2,
                                       unsigned int *exit_status) noexcept;

  /*! Overload of `stop` with `c2` and `c3` set to `noop` and `t2` and `t3 set
  to 0. */
  REPROCXX_EXPORT std::error_code stop(cleanup c1, reproc::milliseconds t1,
                                       unsigned int *exit_status) noexcept;

private:
  std::unique_ptr<reproc_type> process_;
  bool running_;

  cleanup c1_;
  reproc::milliseconds t1_;
  cleanup c2_;
  reproc::milliseconds t2_;
  cleanup c3_;
  reproc::milliseconds t3_;

  static constexpr unsigned int BUFFER_SIZE = 1024;
};

template <typename Parser>
std::error_code process::parse(reproc::stream stream, Parser &&parser)
{
  // A single call to `read` might contain multiple messages. By always calling
  // `parser` once with no data before reading, we give it the chance to process
  // all previous output one by one before reading from the child process again.
  if (!parser("", 0)) {
    return {};
  }

  char buffer[BUFFER_SIZE]; // NOLINT
  std::error_code ec;

  while (true) {
    unsigned int bytes_read = 0;
    ec = read(stream, buffer, BUFFER_SIZE, &bytes_read);
    if (ec) {
      break;
    }

    // `parser` returns false to tell us to stop reading.
    if (!parser(buffer, bytes_read)) {
      break;
    }
  }

  return ec;
}

template <typename Sink>
std::error_code process::drain(reproc::stream stream, Sink &&sink)
{
  char buffer[BUFFER_SIZE]; // NOLINT
  std::error_code ec;

  while (true) {
    unsigned int bytes_read = 0;
    ec = read(stream, buffer, BUFFER_SIZE, &bytes_read);
    if (ec) {
      break;
    }

    // `sink` return false to tell us to stop reading.
    if (!sink(buffer, bytes_read)) {
      break;
    }
  }

  // The child process closing the stream is not treated as an error.
  if (ec == reproc::errc::stream_closed) {
    return {};
  }

  return ec;
}

} // namespace reproc

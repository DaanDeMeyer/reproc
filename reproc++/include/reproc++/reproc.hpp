#pragma once

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
exposing reproc's API when using reproc++ all the other enums and constants of
reproc have a replacement in reproc++ as well. */
namespace reproc {

/*! See `REPROC_STREAM` */
enum class stream {
  /*! `REPROC_STREAM_IN` */
  in = 0,
  /*! `REPROC_STREAM_OUT` */
  out = 1,
  /*! `REPROC_STREAM_ERR` */
  err = 2
};

using milliseconds = std::chrono::duration<unsigned int, std::milli>;
/*! See `REPROC_INFINITE` */
REPROCXX_EXPORT extern const milliseconds infinite;

/*! See `process::stop` */
enum class cleanup {
  /*! Do nothing (no operation). */
  noop = 0,
  /*! `process::wait` */
  wait = 1,
  /*! `process::terminate` */
  terminate = 2,
  /*! `process::kill` */
  kill = 3
};

namespace detail {

template <bool B, typename T = void>
using enable_if = typename std::enable_if<B, T>::type;

template <typename T>
using is_char_array = std::is_convertible<T, const char *const *>;

template <typename T>
using enable_if_not_char_array = enable_if<!is_char_array<T>::value>;

} // namespace detail

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
  REPROCXX_EXPORT process(process &&) noexcept;
  REPROCXX_EXPORT process &operator=(process &&) noexcept;

  /*! `reproc_start` */
  REPROCXX_EXPORT std::error_code
  start(const char *const *argv,
        const char *const *environment = nullptr,
        const char *working_directory = nullptr) noexcept;

  /*!
  Overload of `start` for convenient usage with C++ containers of strings.

  `Arguments` must be a `Container` containing `SequenceContainer` containing
  characters. Examples of types that satisfy this requirement are
  `std::vector<std::string>` and `std::array<std::string>`.

  `Environment` must be a `Container` containing `pair` of `SequenceContainer`
  containing characters. Examples of types that satisfy this requirement are
  `std::vector<std::pair<std::string, std::string>>` and
  `std::map<std::string, std::string>`.

  - `Container`: https://en.cppreference.com/w/cpp/named_req/Container
  - `SequenceContainer`:
  https://en.cppreference.com/w/cpp/named_req/SequenceContainer
  - `pair`: Anything that resembles `std::pair`
  (https://en.cppreference.com/w/cpp/utility/pair).

  `arguments` has the same restrictions as `argv` in `reproc_start` except that
  it should not end with `NULL` (`start` allocates a new array which includes
  the missing `NULL` value).

  The pairs in `environment` represent the environment variables of the child
  process and are converted to the right format before being passed as the
  environment to `reproc_start`. To have the child process inherit the
  environment of the parent process, use the `start` overload that does not have
  an environment parameter.

  `working_directory` specifies the working directory. It is optional and
  defaults to `nullptr`.

  This method only participates in overload resolution if `Arguments` and
  `Environment` are not convertible to `const char *const *`.
  */
  template <typename Arguments,
            typename Environment,
            typename = detail::enable_if_not_char_array<Arguments>,
            typename = detail::enable_if_not_char_array<Environment>>
  std::error_code start(const Arguments &arguments,
                        const Environment &environment,
                        const char *working_directory = nullptr);

  /*! Same as `start` but instead of passing a custom environment, the child
  process inherits the environment of the parent process. */
  template <typename Arguments,
            typename = detail::enable_if_not_char_array<Arguments>>
  std::error_code start(const Arguments &arguments,
                        const char *working_directory = nullptr);

  /*! `reproc_read` */
  REPROCXX_EXPORT std::error_code read(stream stream,
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
  bool parser(const uint8_t *buffer, unsigned int size);
  ```
  */
  template <typename Parser>
  std::error_code parse(stream stream, Parser &&parser);

  /*!
  `parse` but `stream_closed` is not treated as an error. `Sink` expects the
  same signature as `Parser` in `parse`.

  For examples of sinks, see `sink.hpp`
  */
  template <typename Sink>
  std::error_code drain(stream stream, Sink &&sink);

  /*! `reproc_write` */
  REPROCXX_EXPORT std::error_code write(const uint8_t *buffer,
                                        unsigned int size,
                                        unsigned int *bytes_written) noexcept;

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
  std::unique_ptr<reproc_t> process_;

  cleanup c1_;
  milliseconds t1_;
  cleanup c2_;
  milliseconds t2_;
  cleanup c3_;
  milliseconds t3_;

  static constexpr unsigned int BUFFER_SIZE = 1024;

  class REPROCXX_EXPORT arguments {
    char **data_;

  public:
    template <typename Arguments>
    explicit arguments(const Arguments &arguments);

    arguments(const arguments &) = delete;

    ~arguments();

    const char *const *data() const noexcept;
  };

  class REPROCXX_EXPORT environment {
    char **data_;

  public:
    template <typename Environment>
    explicit environment(const Environment &environment);

    environment(const environment &) = delete;

    ~environment();

    const char *const *data() const noexcept;
  };
};

template <typename Arguments, typename Environment, typename, typename>
std::error_code process::start(const Arguments &arguments,
                               const Environment &environment,
                               const char *working_directory)
{
  process::arguments args(arguments);
  process::environment env(environment);

  return start(args.data(), env.data(), working_directory);
}

template <typename Arguments, typename>
std::error_code process::start(const Arguments &arguments,
                               const char *working_directory)
{
  process::arguments args(arguments);

  return start(args.data(), nullptr, working_directory);
}

template <typename Parser>
std::error_code process::parse(stream stream, Parser &&parser)
{
  // A single call to `read` might contain multiple messages. By always calling
  // `parser` once with no data before reading, we give it the chance to process
  // all previous output one by one before reading from the child process again.
  if (!parser(nullptr, 0)) {
    return {};
  }

  uint8_t buffer[BUFFER_SIZE]; // NOLINT
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
std::error_code process::drain(stream stream, Sink &&sink)
{
  std::error_code ec = parse(stream, std::forward<Sink>(sink));

  return ec == error::stream_closed ? error::success : ec;
}

template <typename Arguments>
process::arguments::arguments(const Arguments &arguments)
    : data_(new char *[arguments.size() + 1]) // NOLINT
{
  using value_size_type = typename Arguments::value_type::size_type;

  size_t current = 0;

  for (const auto &entry : arguments) {
    auto string = new char[entry.size() + 1]; // NOLINT

    data_[current++] = string;

    for (value_size_type i = 0; i < entry.size(); i++) {
      *string++ = entry[i];
    }

    *string = '\0';
  }

  data_[current] = nullptr;
}

template <typename Environment>
process::environment::environment(const Environment &environment)
    : data_(new char *[environment.size() + 1]) // NOLINT
{
  using key_size_type = typename Environment::value_type::first_type::size_type;
  using value_size_type =
      typename Environment::value_type::second_type::size_type;

  size_t current = 0;

  for (const auto &entry : environment) {
    // +2 => '=' + '\0'
    size_t size = entry.first.size() + entry.second.size() + 2;
    auto string = new char[size]; // NOLINT

    data_[current++] = string;

    for (key_size_type i = 0; i < entry.first.size(); i++) {
      *string++ = entry.first[i];
    }

    *string++ = '=';

    for (value_size_type i = 0; i < entry.second.size(); i++) {
      *string++ = entry.second[i];
    }

    *string = '\0';
  }

  data_[current] = nullptr;
}

} // namespace reproc

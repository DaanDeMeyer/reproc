/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include "reproc++/error.hpp"
#include "reproc++/export.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

// Forward declare reproc_type so we don't have to include reproc.h in the
// header.
struct reproc_type;

/*! The `reproc` namespace wraps all reproc++ declarations. reproc::process
wraps reproc's API inside a C++ class. reproc::errc improves on #REPROC_ERROR by
integrating with C++'s std::error_code error handling mechanism. To avoid
exposing reproc's API when using reproc++ all the other enums and constants of
reproc have a replacement in reproc++ as well. */
namespace reproc
{

/*! \see REPROC_STREAM */
enum class stream {
  /*! #REPROC_IN */
  in = 0,
  /*! #REPROC_OUT */
  out = 1,
  /*! #REPROC_ERR */
  err = 2
};

using milliseconds = std::chrono::duration<unsigned int, std::milli>;
/*! \see REPROC_INFINITE */
REPROC_EXPORT extern const reproc::milliseconds infinite;

/*! \see process::stop */
enum cleanup {
  /*! Do nothing (no operation). */
  noop = 0,
  /*! \ref process::wait */
  wait = 1,
  /*! \ref process::terminate */
  terminate = 2,
  /*! \ref process::kill */
  kill = 3
};

/*! Improves on reproc's API by wrapping it in a class. Aside from methods that
mimick reproc's API it also adds configurable RAII and several methods that
reduce the boilerplate required to use reproc from idiomatic C++ code. */
class process
{

public:
  /*!
  Allocates memory for the #reproc_type struct. Throws std::bad_alloc if
  allocating memory for the #reproc_type struct of the underlying C library
  fails.

  The given arguments are passed to #stop in the destructor if the process is
  still running.

  Example:

  \code{.cpp}
  reproc::process example(reproc::wait, 10000, reproc::terminate, 5000);
  \endcode

  If the child process is still running when example's destructor is called, it
  will first wait 10 seconds for the child process to exit on its own before
  sending `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waiting 5 more seconds
  for the child process to exit.

  By default the destructor waits indefinitely for the child process to exit.
  */
  REPROC_EXPORT process(cleanup c1 = reproc::wait,
                        reproc::milliseconds t1 = reproc::infinite);

  REPROC_EXPORT process(cleanup c1, reproc::milliseconds t1, cleanup c2,
                        reproc::milliseconds t2);

  REPROC_EXPORT process(cleanup c1, reproc::milliseconds t1, cleanup c2,
                        reproc::milliseconds t2, cleanup c3,
                        reproc::milliseconds t3);

  /*! Calls #stop with the arguments provided in the constructor if the child
  process is still running and frees all allocated resources. */
  REPROC_EXPORT ~process() noexcept;

  // Enforce unique ownership of child processes.

  process(const process &) = delete;
  process &operator=(const process &) = delete;

  REPROC_EXPORT process(process &&) noexcept = default;
  REPROC_EXPORT process &operator=(process &&) noexcept = default;

  /*! \see reproc_start */
  REPROC_EXPORT std::error_code
  start(int argc, const char *const *argv,
        const char *working_directory = nullptr) noexcept;

  /*!
  Overload of #start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in #reproc_start except
  that it should not end with `NULL` (this method allocates a new array which
  includes the missing `NULL` value).
  \param[in] working_directory Optional working directory. Defaults to
  `nullptr`.

  \see reproc_start
  */
  REPROC_EXPORT std::error_code
  start(const std::vector<std::string> &args,
        const std::string *working_directory = nullptr);

  /*! \see reproc_read */
  REPROC_EXPORT std::error_code read(reproc::stream stream, void *buffer,
                                     unsigned int size,
                                     unsigned int *bytes_read) noexcept;

  /*!
  Calls #read until the provided parser returns false or an error occurs. \p
  parser receives the output after each read.

  \p parser is always called once with an empty string to give the parser the
  chance to process all output from the previous call to #read one by one.

  \tparam Parser
  \parblock
  \p Parser should have the following signature:

  \code{.cpp}
  bool parser(const char *buffer, unsigned int size);
  \endcode
  \endparblock

  \param[in] stream The stream to read from.
  \param[in] parser An instance of \p Parser.
  */
  template <typename Parser>
  std::error_code read(reproc::stream stream, Parser &&parser);

  /*!
  Calls #read until the stream is closed or an error occurs. \p sink receives
  the output after each read.

  Note that this method does not report the output stream being closed as an
  error.

  \tparam Sink
  \parblock
  \p Sink should have the following signature:

  \code{.cpp}
  void sink(const char *buffer, unsigned int size);
  \endcode

  For examples of sinks, see sink.hpp
  \endparblock

  \param[in] stream The stream to read from.
  \param[in] sink An instance of \p Sink.
  */
  template <typename Sink>
  std::error_code drain(reproc::stream stream, Sink &&sink);

  /*! \see reproc_write */
  REPROC_EXPORT std::error_code write(const void *buffer, unsigned int to_write,
                                      unsigned int *bytes_written) noexcept;

  /*! \see reproc_close */
  REPROC_EXPORT void close(reproc::stream stream) noexcept;

  /*! \see reproc_wait */
  REPROC_EXPORT std::error_code wait(reproc::milliseconds timeout,
                                     unsigned int *exit_status) noexcept;

  /*! \see reproc_terminate */
  REPROC_EXPORT std::error_code terminate(reproc::milliseconds timeout,
                                          unsigned int *exit_status) noexcept;

  /*! \see reproc_kill */
  REPROC_EXPORT std::error_code kill(reproc::milliseconds timeout,
                                     unsigned int *exit_status) noexcept;

  /*! \see reproc_stop */
  REPROC_EXPORT std::error_code stop(cleanup c1, reproc::milliseconds t1,
                                     cleanup c2, reproc::milliseconds t2,
                                     cleanup c3, reproc::milliseconds t3,
                                     unsigned int *exit_status) noexcept;

  /*! Overload of #stop with \p c3 set to #noop and \p t3 set to 0. */
  REPROC_EXPORT std::error_code stop(cleanup c1, reproc::milliseconds t1,
                                     cleanup c2, reproc::milliseconds t2,
                                     unsigned int *exit_status) noexcept;
  /*! Overload of #stop with \p c2 and c3 set to #noop and \p t2 and \p t3 set
  to 0. */
  REPROC_EXPORT std::error_code stop(cleanup c1, reproc::milliseconds t1,
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
std::error_code process::read(reproc::stream stream, Parser &&parser)
{
  /* A single call to read might contain multiple messages. By always calling
  the parser once with no data before reading, we give the parser the chance to
  process all previous output one by one before reading from the child process
  again. */
  if (!parser("", 0)) { return {}; }

  char buffer[BUFFER_SIZE];
  std::error_code ec;

  while (true) {
    unsigned int bytes_read = 0;
    ec = read(stream, buffer, BUFFER_SIZE, &bytes_read);
    if (ec) { break; }

    // parser returns false to tell us to stop reading.
    if (!parser(buffer, bytes_read)) { break; }
  }

  return ec;
}

template <typename Sink>
std::error_code process::drain(reproc::stream stream, Sink &&sink)
{
  char buffer[BUFFER_SIZE];
  std::error_code ec;

  while (true) {
    unsigned int bytes_read = 0;
    ec = read(stream, buffer, BUFFER_SIZE, &bytes_read);
    if (ec) { break; }

    sink(buffer, bytes_read);
  }

  // The child process closing the stream is not treated as an error.
  if (ec == reproc::errc::stream_closed) { return {}; }

  return ec;
}

} // namespace reproc

#endif

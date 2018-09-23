/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include "error.hpp"
#include "export.hpp"

#include <memory>
#include <string>
#include <vector>

/*! The `reproc` namespace wraps all reproc C++ declarations. reproc::process
wraps the C api inside a C++ class. reproc::errc improves on #REPROC_ERROR by
integrating with C++'s std::error_code error handling mechanism. To avoid
exposing the C API when using the C++ API all the other enums and constants of
the C API have a replacement in the `reproc` namespace as well. */
namespace reproc
{

/*! \see REPROC_STREAM */
enum class stream {
  /*! #REPROC_IN */
  in,
  /*! #REPROC_OUT */
  out,
  /*! #REPROC_ERR */
  err
};

/*! \see REPROC_INFINITE */
REPROC_EXPORT extern const unsigned int infinite;

/*! \see REPROC_CLEANUP */
enum class cleanup {
  /*! #REPROC_WAIT */
  wait = 1 << 0,
  /*! #REPROC_TERMINATE */
  terminate = 1 << 1,
  /*! #REPROC_KILL */
  kill = 1 << 2
};

/*! Used to combine multiple flags from reproc::cleanup. */
REPROC_EXPORT reproc::cleanup operator|(reproc::cleanup lhs,
                                        reproc::cleanup rhs) noexcept;

/*! Improves on reproc's C API by wrapping it in a class. Aside from methods
that mimick the C API it also adds configurable RAII and several methods that
reduce the boilerplate required when using reproc from idiomatic C++ code. */
class process
{

public:
  /*! Allocates memory for the #reproc_type struct. Throws std::bad_alloc if
  allocating memory for the #reproc_type struct of the underlying C library
  fails.

  Takes arguments that are passed to #stop in the destructor if the process is
  still running by the time the object is destroyed. The default flags and
  timeout value only tell the destructor to check if the process has exited.
  Pass different flags and timeout values if the destructor should wait longer
  for the process to exit or if the process should be stopped forcefully in the
  destructor.

  \see reproc_stop
  */
  REPROC_EXPORT process(reproc::cleanup cleanup_flags = reproc::cleanup::wait,
                        unsigned int timeout = 0);

  /*! Frees the allocated memory for the #reproc_type struct and calls #stop
  with the arguments provided in the constructor if #stop hasn't been called
  explicitly yet. */
  REPROC_EXPORT ~process() noexcept;

  // Enforce unique ownership
  process(const process &) = delete;
  process &operator=(const process &) = delete;

  REPROC_EXPORT process(process &&other) noexcept = default;
  REPROC_EXPORT process &operator=(process &&other) noexcept = default;

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

  \return reproc::errc

  \see reproc_start
  */
  REPROC_EXPORT std::error_code
  start(const std::vector<std::string> &args,
        const std::string *working_directory = nullptr);

  /*! \see reproc_write */
  REPROC_EXPORT std::error_code write(const void *buffer, unsigned int to_write,
                                      unsigned int *bytes_written) noexcept;

  /*! \see reproc_close */
  REPROC_EXPORT void close(reproc::stream stream) noexcept;

  /*! \see reproc_read */
  REPROC_EXPORT std::error_code read(reproc::stream stream, void *buffer,
                                     unsigned int size,
                                     unsigned int *bytes_read) noexcept;

  /*!
  Calls #read until an error occurs or the provided parser returns false.

  \tparam Parser
  \parblock
  Should be a function with the following signature:

  \code{.cpp}
  bool parser(const char *buffer, unsigned int size);
  \endcode

  The parser receives the buffer after each read so it can be appended to the
  final result. One way to make a parser is to use a lambda:

  \code{.cpp}
  reproc::process process;
  process.start(...)

  std::string output;
  process.read(reproc::stream::cout,
               [&output](const char *buffer, unsigned int size) {
    output.append(buffer, size);
    return true;
  });
  \endcode

  This parser reads the entire output of the child process into a string.

  It is also possible to use a class that overloads the call operator as a
  parser. parser.hpp contains built-in parsers that are defined as a class.

  Note that this method does not report the child process closing the output
  stream as an error.

  For examples of parsers, see parser.hpp.
  \endparblock

  \param stream Stream to read from
  \param parser Instance of \p Parser

  \return reproc::errc

  Possible errors: See #read except for reproc::errc::stream_closed
  */
  template <typename Parser>
  std::error_code read(reproc::stream stream, Parser &&parser);

  /*! \see reproc_stop */
  REPROC_EXPORT std::error_code stop(reproc::cleanup cleanup_flags,
                                     unsigned int timeout,
                                     unsigned int *exit_status) noexcept;

private:
  reproc::cleanup cleanup_flags_;
  unsigned int timeout_;
  std::unique_ptr<struct reproc_type> process_;
  bool running_;
};

template <typename Parser>
std::error_code process::read(reproc::stream stream, Parser &&parser)
{
  static constexpr unsigned int BUFFER_SIZE = 1024;

  char buffer[BUFFER_SIZE];
  std::error_code ec;

  while (true) {
    unsigned int bytes_read = 0;
    ec = read(stream, buffer, BUFFER_SIZE, &bytes_read);
    if (ec) { break; }

    // parser returns false to tell us to stop reading
    if (!parser(buffer, bytes_read)) { break; }
  }

  // The child process closing the stream is not treated as an error
  if (ec == reproc::errc::stream_closed) { return {}; }

  return ec;
}

} // namespace reproc

#endif

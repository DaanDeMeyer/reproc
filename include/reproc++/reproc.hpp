/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include "reproc++/error.hpp"
#include "reproc++/export.hpp"

#include <memory>
#include <string>
#include <vector>

// Forward reproc_type so we don't have to include reproc.h in the header.
struct reproc_type;

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

enum cleanup {
  /*! Do nothing. */
  none = 0,
  /*! \see #process::wait */
  wait = 1,
  /*! \see #process::terminate */
  terminate = 2,
  /*! \see #process::kill */
  kill = 3
};

/*! Improves on reproc's C API by wrapping it in a class. Aside from methods
that mimick the C API it also adds configurable RAII and several methods that
reduce the boilerplate required when using reproc from idiomatic C++ code. */
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
  sending `SIGTERM` (POSIX) or `CTRL-BREAK` Windows and waiting 5 more seconds
  for the child process to exit.

  By default the destructor waits indefinitely for the child process to exit.
  */
  REPROC_EXPORT process(cleanup c1 = reproc::wait,
                        unsigned int t1 = reproc::infinite);

  REPROC_EXPORT process(cleanup c1, unsigned int t1, cleanup c2, unsigned t2);

  REPROC_EXPORT process(cleanup c1, unsigned int t1, cleanup c2, unsigned t2,
                        cleanup c3, unsigned int t3);

  /*! Calls #stop with the arguments provided in the constructor if the child
  process is still running and frees all allocated resources. */
  REPROC_EXPORT ~process() noexcept;

  // Enforce unique ownership of process objects.
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

  \param stream The stream to read from.
  \param parser An instance of \p Parser.

  \return reproc::errc

  Possible errors: See #read except for reproc::errc::stream_closed.
  */
  template <typename Parser>
  std::error_code read(reproc::stream stream, Parser &&parser);

  /*!
  Simplifies calling combinations of #wait, #terminate and #kill.

  Example:

  Wait 10 seconds for the child process to exit on its own before sending
  `SIGTERM` (POSIX) or `CTRL-BREAK` (Windows) and waiting 5 more seconds for the
  child process to exit.

  \code{.cpp}
  std::error_code ec = process.stop(reproc::wait, 10000,
                                    reproc::terminate, 5000);
  \endcode

  Call #wait, #terminate and #kill directly if you need extra logic such as
  logging between calls.
  */
  REPROC_EXPORT std::error_code stop(cleanup c1, unsigned int t1,
                                     unsigned int *exit_status) noexcept;
  REPROC_EXPORT std::error_code stop(cleanup c1, unsigned int t1, cleanup c2,
                                     unsigned int t2,
                                     unsigned int *exit_status) noexcept;
  REPROC_EXPORT std::error_code stop(cleanup c1, unsigned int t1, cleanup c2,
                                     unsigned int t2, cleanup c3,
                                     unsigned int t3,
                                     unsigned int *exit_status) noexcept;

  /*! \see reproc_wait */
  REPROC_EXPORT std::error_code wait(unsigned int timeout,
                                     unsigned int *exit_status) noexcept;

  /*! \see reproc_terminate */
  REPROC_EXPORT std::error_code terminate(unsigned int timeout,
                                          unsigned int *exit_status) noexcept;

  /*! \see reproc_kill */
  REPROC_EXPORT std::error_code kill(unsigned int timeout,
                                     unsigned int *exit_status) noexcept;

private:
  std::unique_ptr<reproc_type> process_;
  bool running_;

  cleanup c1_;
  unsigned int t1_;
  cleanup c2_;
  unsigned int t2_;
  cleanup c3_;
  unsigned int t3_;
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

    // parser returns false to tell us to stop reading.
    if (!parser(buffer, bytes_read)) { break; }
  }

  // The child process closing the stream is not treated as an error.
  if (ec == reproc::errc::stream_closed) { return {}; }

  return ec;
}

} // namespace reproc

#endif

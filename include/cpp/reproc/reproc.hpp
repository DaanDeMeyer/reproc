/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include "error.hpp"
#include "export.hpp"

#include <memory>
#include <string>
#include <vector>

namespace reproc
{

enum class stream { in, out, err };

REPROC_EXPORT extern const unsigned int infinite;

enum class cleanup : int { wait = 1 << 0, terminate = 1 << 1, kill = 1 << 2 };

reproc::cleanup operator|(reproc::cleanup lhs, reproc::cleanup rhs);

class process
{

public:
  /*! Allocates memory for the reproc_type struct. Throws std::bad_alloc if
  allocating memory for the reproc_type struct of the underlying C library
  fails. */
  REPROC_EXPORT process();

  /*! The destructor does not stop the child process if it still running. Make
  sure the process has stopped before the destructor is called by using \see
  stop. */
  REPROC_EXPORT ~process() noexcept;

  /* Enforce unique ownership */
  process(const process &) = delete;
  process &operator=(const process &) = delete;

  REPROC_EXPORT process(process &&other) noexcept = default;
  REPROC_EXPORT process &operator=(process &&other) noexcept = default;

  /*! \see reproc_start. /p working_directory additionally defaults to nullptr
   */
  REPROC_EXPORT std::error_code
  start(int argc, const char *const *argv,
        const char *working_directory = nullptr) noexcept;

  /*!
  Overload of start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in \see reproc_start except
  that it should not end with NULL (this method allocates a new array which
  includes the missing NULL value).
  \param[in] working_directory Optional working directory. Defaults to nullptr.

  \return reproc::error \see process_start
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
  Calls \see read until an error occurs or the provided parser returns false.

  /p parser should be a function with the following signature:

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

  This parser reads all the output of the child process into a string.

  It is also possible to use a class that overloads the call operator as a
  parser. parser.hpp contains built-in parsers that are defined as a class.

  Note that this method does not report the child process closing the output
  stream as an error.

  For examples of parsers, see parser.hpp.

  \return reproc::error \see read except for reproc::error::stream_closed
  */
  template <typename Parser>
  std::error_code read(reproc::stream stream, Parser &&parser);

  /*! \see reproc_stop */
  REPROC_EXPORT std::error_code stop(reproc::cleanup cleanup_flags,
                                     unsigned int timeout,
                                     unsigned int *exit_status);

private:
  std::unique_ptr<struct reproc_type> process_;
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
  if (ec == reproc::error::stream_closed) { return {}; }

  return ec;
}

} // namespace reproc

#endif

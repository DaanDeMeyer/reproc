/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include "error.hpp"
#include "export.hpp"

#include <memory>
#include <string>
#include <vector>

namespace reproc {

enum Stream { STDIN, STDOUT, STDERR };

REPROC_EXPORT extern const unsigned int INFINITE;

class Reproc {

public:
  /*! \see reproc_init. Throws std::bad_alloc if allocating memory for the
  reproc struct of the underlying C library fails */
  REPROC_EXPORT Reproc();
  /*! \see reproc_destroy */
  REPROC_EXPORT ~Reproc();

  /* Enforce unique ownership */
  Reproc(const Reproc &) = delete;
  Reproc &operator=(const Reproc &) = delete;

  REPROC_EXPORT Reproc(Reproc &&other) noexcept = default;
  REPROC_EXPORT Reproc &operator=(Reproc &&other) = default;

  /*! \see reproc_start. /p working_directory additionally defaults to nullptr
   */
  REPROC_EXPORT Error start(int argc, const char *const *argv,
                            const char *working_directory = nullptr);

  /*!
  Overload of start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in \see reproc_start except
  that it should not end with NULL (the method allocates a new array which
  includes the missing NULL value).
  \param[in] working_directory Optional working directory. Defaults to nullptr.
  */
  REPROC_EXPORT Error start(const std::vector<std::string> &args,
                            const std::string *working_directory = nullptr);

  /*! \see reproc_write */
  REPROC_EXPORT Error write(const void *buffer, unsigned int to_write,
                            unsigned int *bytes_written);

  /*! \see reproc_close */
  REPROC_EXPORT Error close(Stream stream);

  /*! \see reproc_read */
  REPROC_EXPORT Error read(Stream stream, void *buffer, unsigned int size,
                           unsigned int *bytes_read);

  /*!
  Calls \see read until an error occurs or the provided parser returns false.

  Takes a Parser with the following signature:

  \code{.cpp}
  class Parser {
    // If stream_closed_is_error returns false, SUCCESS is returned instead of
    // STREAM_CLOSED when the stream is closed
    bool stream_closed_is_error();

    // Receives the buffer after each read so it can be parsed and appended
    // to the final result
    bool operator()(const char *buffer, unsigned int size);
  }
  bool parser(const char *buffer, unsigned int size);
  \endcode

  The parser receives the buffer after each read so it can append it to the
  final result.

  For examples of parsers, see parser.hpp.

  \return Error \see reproc_read
  */
  template <typename Parser> Error read(Stream stream, Parser &&parser);

  /*! \see reproc_wait */
  REPROC_EXPORT Error wait(unsigned int milliseconds);

  /*! \see reproc_terminate */
  REPROC_EXPORT Error terminate(unsigned int milliseconds);

  /*! \see reproc_kill */
  REPROC_EXPORT Error kill(unsigned int milliseconds);

  /*! \see reproc_exit_status */
  REPROC_EXPORT Error exit_status(int *exit_status);

private:
  std::unique_ptr<struct reproc_type> reproc;
};

template <typename Parser> Error Reproc::read(Stream stream, Parser &&parser)
{
  static constexpr unsigned int BUFFER_SIZE = 1024;

  char buffer[BUFFER_SIZE];
  Error error = SUCCESS;

  while (true) {
    unsigned int bytes_read = 0;
    error = read(stream, buffer, BUFFER_SIZE, &bytes_read);
    if (error) { break; }

    // parser returns false to tell us to stop reading
    if (!parser(buffer, bytes_read)) { break; }
  }

  if (error == STREAM_CLOSED && !parser.stream_closed_is_error()) {
    return SUCCESS;
  }

  return error;
}

} // namespace reproc

#endif

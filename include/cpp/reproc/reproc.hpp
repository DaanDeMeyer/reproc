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

enum stream { cin, cout, cerr };

REPROC_EXPORT extern const unsigned int infinite;

class process
{

public:
  /*! Allocates memory for the reproc_type struct. Throws std::bad_alloc if
  allocating memory for the reproc_type struct of the underlying C library
  fails. */
  REPROC_EXPORT process();

  /*!
  see reproc_destroy. Aditionally frees the memory allocated in the
  constructor.

  The destructor does not stop the child process if it still running. Make sure
  the process has stopped before the destructor is called by using a combination
  of \see wait, \see terminate and \see kill.
  */
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

  Takes a Parser with the following signature:

  \code{.cpp}
  struct Parser {
    // If stream_closed_is_error returns false, no error is returned when the
    // stream is closed. This avoids having to check for
    // reproc::error::stream_closed after read returns.
    bool stream_closed_is_error();

    // Receives the buffer after each read so it can be parsed and appended
    // to the final result
    bool operator()(const char *buffer, unsigned int size);
  }
  \endcode

  The parser receives the buffer after each read so it can be appended to the
  final result.

  For examples of parsers, see parser.hpp.

  \return reproc::error \see reproc_read
  */
  template <typename Parser>
  std::error_code read(reproc::stream stream, Parser &&parser);

  /*! \see reproc_wait */
  REPROC_EXPORT std::error_code wait(unsigned int milliseconds,
                                     unsigned int *exit_status) noexcept;

  /*! \see reproc_terminate */
  REPROC_EXPORT std::error_code terminate(unsigned int milliseconds) noexcept;

  /*! \see reproc_kill */
  REPROC_EXPORT std::error_code kill(unsigned int milliseconds) noexcept;

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

  if (ec == reproc::error::stream_closed && !parser.stream_closed_is_error()) {
    return {}; // success
  }

  return ec;
}

} // namespace reproc

#endif

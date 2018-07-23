/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include "error.hpp"
#include "export.hpp"

#include <memory>
#include <string>
#include <vector>

namespace reproc {

enum stream { cin, cout, cerr };

REPROC_EXPORT extern const unsigned int infinite;

class process {

public:
  /*! \see reproc_init. Aditionally allocates memory for the process_type
  struct. Throws std::bad_alloc if allocating memory for the process_type struct
  of the underlying C library fails. */
  REPROC_EXPORT process();

  /*! \see reproc_destroy. Aditionally frees the memory allocated in the
  constructor. */
  REPROC_EXPORT ~process();

  /* Enforce unique ownership */
  process(const process &) = delete;
  process &operator=(const process &) = delete;

  REPROC_EXPORT process(process &&other) noexcept = default;
  REPROC_EXPORT process &operator=(process &&other) = default;

  /*! \see reproc_start. /p working_directory additionally defaults to nullptr
   */
  REPROC_EXPORT reproc::error start(int argc, const char *const *argv,
                                    const char *working_directory = nullptr);

  /*!
  Overload of start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in \see reproc_start except
  that it should not end with NULL (this method allocates a new array which
  includes the missing NULL value).
  \param[in] working_directory Optional working directory. Defaults to nullptr.

  \return reproc::error \see process_start
  */
  REPROC_EXPORT reproc::error
  start(const std::vector<std::string> &args,
        const std::string *working_directory = nullptr);

  /*! \see reproc_write */
  REPROC_EXPORT reproc::error write(const void *buffer, unsigned int to_write,
                                    unsigned int *bytes_written);

  /*! \see reproc_close */
  REPROC_EXPORT void close(reproc::stream stream);

  /*! \see reproc_read */
  REPROC_EXPORT reproc::error read(reproc::stream stream, void *buffer,
                                   unsigned int size, unsigned int *bytes_read);

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

  The parser receives the buffer after each read so it can be appended to the
  final result.

  For examples of parsers, see parser.hpp.

  \return reproc::error \see reproc_read
  */
  template <typename Parser>
  reproc::error read(reproc::stream stream, Parser &&parser);

  /*! \see reproc_wait */
  REPROC_EXPORT reproc::error wait(unsigned int milliseconds,
                                   unsigned int *exit_status);

  /*! \see reproc_terminate */
  REPROC_EXPORT reproc::error terminate(unsigned int milliseconds);

  /*! \see reproc_kill */
  REPROC_EXPORT reproc::error kill(unsigned int milliseconds);

private:
  std::unique_ptr<struct reproc_type> process_;
};

template <typename Parser>
reproc::error process::read(reproc::stream stream, Parser &&parser)
{
  static constexpr unsigned int BUFFER_SIZE = 1024;

  char buffer[BUFFER_SIZE];
  reproc::error error = reproc::success;

  while (true) {
    unsigned int bytes_read = 0;
    error = read(stream, buffer, BUFFER_SIZE, &bytes_read);
    if (error) { break; }

    // parser returns false to tell us to stop reading
    if (!parser(buffer, bytes_read)) { break; }
  }

  if (error == reproc::stream_closed && !parser.stream_closed_is_error()) {
    return success;
  }

  return error;
}

} // namespace reproc

#endif

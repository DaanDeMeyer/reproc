/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "error.hpp"

#if defined(_WIN32) && defined(REPROC_SHARED)
#if defined(REPROC_BUILDING)
#define REPROC_EXPORT __declspec(dllexport)
#else
#define REPROC_EXPORT __declspec(dllimport)
#endif
#else
#define REPROC_EXPORT
#endif

namespace reproc {

class Reproc {
private:
  // Helpers used to remove the read_all/read_all_stderr template functions from
  // overload resolution when an ostream ref is passed as a parameter so the
  // ostream & overload of read_all/read_all_stderr is used instead.
  template <bool B, typename T = void>
  using enable_if_t = typename std::enable_if<B, T>::type;

  template <typename T>
  using is_ostream_ref = std::is_convertible<T, std::ostream &>;

public:
  REPROC_EXPORT static const unsigned int INFINITE;

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
  REPROC_EXPORT reproc::error start(int argc, const char *const *argv,
                                    const char *working_directory = nullptr);

  /*!
  Overload of start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in \see reproc_start except
  that it should not end with NULL (the method allocates a new array which
  includes the missing NULL value).
  \param[in] working_directory Optional working directory. Defaults to nullptr.
  */
  REPROC_EXPORT reproc::error
  start(const std::vector<std::string> &args,
        const std::string *working_directory = nullptr);

  /*! \see reproc_write */
  REPROC_EXPORT reproc::error write(const void *buffer, unsigned int to_write,
                                    unsigned int *bytes_written);

  /*! \see reproc_close_stdin */
  REPROC_EXPORT reproc::error close_stdin();

  /*! \see reproc_read */
  REPROC_EXPORT reproc::error read(void *buffer, unsigned int size,
                                   unsigned int *bytes_read);

  /*! \see reproc_read_stderr */
  REPROC_EXPORT reproc::error read_stderr(void *buffer, unsigned int size,
                                          unsigned int *bytes_read);

  // Convenience overloads of read_all/read_all_stderr template function
  REPROC_EXPORT reproc::error read_all(std::ostream &output);
  REPROC_EXPORT reproc::error read_all_stderr(std::ostream &output);
  REPROC_EXPORT reproc::error read_all(std::string &output);
  REPROC_EXPORT reproc::error read_all_stderr(std::string &output);

  /*!
  Calls \see read until the child process stdout is closed or an error occurs.

  Takes a Write function with the following signature:

  \code{.cpp}
  void write(const char *buffer, unsigned int size);
  \endcode

  The Write function receives the buffer after each read so it can append it to
  the final result.

  Example usage (don't actually use this, use the overload of read_all for
  std::string instead):

  \code{.cpp}
  std::string output;
  reproc.read_all([&output](const char *buffer, unsigned int size) {
    output.append(buffer, size);
  });
  \endcode

  This function does not participate in overload resolution if \p write is an
  ostream ref. This ensures our overloaded version of read_all for ostreams is
  used.

  \return reproc::error \see reproc_read except for STREAM_CLOSED
  */
  template <typename Write,
            typename = enable_if_t<!is_ostream_ref<Write>::value>>
  reproc::error read_all(Write &&write)
  {
    return read_all(
        [this](void *buffer, unsigned int size, unsigned int *bytes_read) {
          return read(buffer, size, bytes_read);
        },
        std::forward<Write>(write));
  }

  /*! \see read_all for stderr */
  template <typename Write,
            typename = enable_if_t<!is_ostream_ref<Write>::value>>
  reproc::error read_all_stderr(Write &&write)
  {
    return read_all(
        [this](void *buffer, unsigned int size, unsigned int *bytes_read) {
          return read_stderr(buffer, size, bytes_read);
        },
        std::forward<Write>(write));
  }

  /*! \see reproc_wait */
  REPROC_EXPORT reproc::error wait(unsigned int milliseconds);

  /*! \see reproc_terminate */
  REPROC_EXPORT reproc::error terminate(unsigned int milliseconds);

  /*! \see reproc_kill */
  REPROC_EXPORT reproc::error kill(unsigned int milliseconds);

  /*! \see reproc_exit_status */
  REPROC_EXPORT reproc::error exit_status(int *exit_status);

  /*! \see reproc_system_error */
  REPROC_EXPORT static unsigned int system_error();

  /*! \see reproc_error_to_string. This function additionally adds the system
  error to the error string when /p error is Reproc::UNKNOWN_ERROR. */
  REPROC_EXPORT static std::string error_to_string(reproc::error error);

private:
  std::unique_ptr<struct reproc_type> reproc;

  static constexpr unsigned int BUFFER_SIZE = 1024;

  // Read until the stream used by read is closed or an error occurs.
  template <typename Read, typename Write>
  reproc::error read_all(Read &&read, Write &&write)
  {
    char buffer[BUFFER_SIZE];
    reproc::error error = reproc::success;

    while (true) {
      unsigned int bytes_read = 0;
      error = read(buffer, BUFFER_SIZE, &bytes_read);
      if (error) { break; }

      write(buffer, bytes_read);
    }

    if (error != reproc::stream_closed) { return error; }

    return reproc::success;
  }
};

} // namespace reproc

#endif

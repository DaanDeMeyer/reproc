/*! \file reproc.hpp */

#ifndef REPROC_HPP
#define REPROC_HPP

#include <iosfwd>
#include <string>
#include <vector>
#include <memory>

#if defined(_WIN32) && defined(REPROC_SHARED)
#if defined(REPROC_BUILDING)
#define REPROC_EXPORT __declspec(dllexport)
#else
#define REPROC_EXPORT __declspec(dllimport)
#endif
#else
#define REPROC_EXPORT
#endif

class REPROC_EXPORT Reproc {
private:
  // Helpers used to remove the read_all template overload from overload
  // resolution when an ostream ref is passed as a paramter
  template <bool B, class T = void>
  using enable_if_t = typename std::enable_if<B, T>::type;

  template <class T>
  using is_ostream_ref = std::is_convertible<T, std::ostream &>;

public:
  static const unsigned int INFINITE;

  /*! \see REPROC_ERROR */
  /* When editing make sure to change the corresponding enum in reproc.h as
  well */
  enum Error {
    SUCCESS,
    UNKNOWN_ERROR,
    WAIT_TIMEOUT,
    STREAM_CLOSED,
    STILL_RUNNING,
    MEMORY_ERROR,
    PIPE_LIMIT_REACHED,
    INTERRUPTED,
    PROCESS_LIMIT_REACHED,
    INVALID_UNICODE,
    PERMISSION_DENIED,
    SYMLINK_LOOP,
    FILE_NOT_FOUND,
    NAME_TOO_LONG,
    PARTIAL_WRITE
  };

  /*! \see reproc_init. Throws std::bad_alloc if allocating memory for the
  reproc struct of the underlying C library fails */
  Reproc();
  /*! \see reproc_destroy */
  ~Reproc();

  /* Enforce unique ownership */
  Reproc(const Reproc &) = delete;
  Reproc(Reproc &&other) noexcept = default;
  Reproc &operator=(const Reproc &) = delete;
  Reproc &operator=(Reproc &&other) = default;

  /*! \see reproc_start */
  Reproc::Error start(int argc, const char *argv[],
                      const char *working_directory);

  /*!
  Overload of start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in \see reproc_start except
  that it should not end with NULL (which is added in this function).
  */
  Reproc::Error start(const std::vector<std::string> &args,
                      const std::string *working_directory);

  /*! \see reproc_write */
  Reproc::Error write(const void *buffer, unsigned int to_write,
                      unsigned int *bytes_written);

  /*! \see reproc_close_stdin */
  Reproc::Error close_stdin();

  /*! \see reproc_read */
  Reproc::Error read(void *buffer, unsigned int size, unsigned int *bytes_read);

  /*! \see reproc_read_stderr */
  Reproc::Error read_stderr(void *buffer, unsigned int size,
                            unsigned int *bytes_read);

  Reproc::Error read_all(std::ostream &out);
  Reproc::Error read_all_stderr(std::ostream &out);
  Reproc::Error read_all(std::string &out);
  Reproc::Error read_all_stderr(std::string &out);

  /*!
  Calls \see read until the child process stdout is closed or an error occurs.

  Takes a Write function with the following signature:

  \code{.cpp}
  void write(const char *buffer, unsigned int size);
  \endcode

  The Write function receives the buffer after each read so it can append it to
  the final result.

  Example usage (don't actually use this, use /see read_all instead):

  \code{.cpp}
  std::string output;
  reproc.read_all([&output](const char *buffer, unsigned int size) {
    output.append(buffer, size);
  });
  \endcode

  This function does not participate in overload resolution if \p write is a
  ostream& (This ensures our overloaded version of read_all for ostreams is
  used).

  \return Reproc::Error \see reproc_read except for STREAM_CLOSED
  */
  template <class Write, class = enable_if_t<!is_ostream_ref<Write>::value>>
  Reproc::Error read_all(Write &&write)
  {
    return read_all(
        [this](void *buffer, unsigned int size, unsigned int *bytes_read) {
          return read(buffer, size, bytes_read);
        },
        std::forward<Write>(write));
  }

  /*! \see read_all for stderr */
  template <class Write, class = enable_if_t<!is_ostream_ref<Write>::value>>
  Reproc::Error read_all_stderr(Write &&write)
  {
    return read_all(
        [this](void *buffer, unsigned int size, unsigned int *bytes_read) {
          return read_stderr(buffer, size, bytes_read);
        },
        std::forward<Write>(write));
  }

  /*! \see reproc_wait */
  Reproc::Error wait(unsigned int milliseconds);

  /*! \see reproc_terminate */
  Reproc::Error terminate(unsigned int milliseconds);

  /*! \see reproc_kill */
  Reproc::Error kill(unsigned int milliseconds);

  /*! \see reproc_exit_status */
  Reproc::Error exit_status(int *exit_status);

  /*! \see reproc_system_error */
  static unsigned int system_error();

  /*! \see reproc_error_to_string */
  static std::string error_to_string(Reproc::Error error);

private:
  std::unique_ptr<struct reproc> reproc;

  static constexpr unsigned int BUFFER_SIZE = 1024;

  template <class Read, class Write>
  Reproc::Error read_all(Read &&read, Write &&write)
  {
    char buffer[BUFFER_SIZE];
    Reproc::Error error = Reproc::SUCCESS;

    while (true) {
      unsigned int bytes_read = 0;
      error = read(buffer, BUFFER_SIZE, &bytes_read);
      if (error) { break; }

      write(buffer, bytes_read);
    }

    if (error != Reproc::STREAM_CLOSED) { return error; }

    return Reproc::SUCCESS;
  }
};

#endif

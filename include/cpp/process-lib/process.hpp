#pragma once

#include <iosfwd>
#include <string>
#include <vector>

class Process
{
public:
  static const unsigned int INFINITE;

  /*! \see PROCESS_LIB_ERROR */
  /* When editing make sure to change the corresponding enum in process.h as
   * well */
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

  /*! \see process_init. Throws std::bad_alloc if allocating memory for the
  process struct of the underlying C library fails */
  Process();
  /*! \see process_destroy */
  ~Process();

  /* Enforce unique ownership */
  Process(const Process &) = delete;
  Process(Process &&other) noexcept;
  Process &operator=(const Process &) = delete;
  Process &operator=(Process &&other) noexcept;

  /*! \see process_start */
  Process::Error start(int argc, const char *argv[],
                       const char *working_directory);

  /*!
  Overload of start for convenient usage from C++.

  \param[in] args Has the same restrictions as argv in \see process_start except
  that it should not end with NULL (which is added in this function).
  */
  Process::Error start(const std::vector<std::string> &args,
                       const std::string *working_directory);

  /*! \see process_write */
  Process::Error write(const void *buffer, unsigned int to_write,
                       unsigned int *bytes_written);

  /*! \see process_close_stdin */
  Process::Error close_stdin();

  /*! \see process_read */
  Process::Error read(void *buffer, unsigned int size,
                      unsigned int *bytes_read);

  /*! \see process_read_stderr */
  Process::Error read_stderr(void *buffer, unsigned int size,
                             unsigned int *bytes_read);

  /*!
  Calls \see read until the child process stdout is closed or an error occurs.

  \param[out] out Stream that receives all output from stdout.

  \return Process::Error \see process_read except for STREAM_CLOSED
  */
  Process::Error read_all(std::ostream &out);

  /*! \see read_all for stderr */
  Process::Error read_all_stderr(std::ostream &out);

  /*! \see process_wait */
  Process::Error wait(unsigned int milliseconds);

  /*! \see process_terminate */
  Process::Error terminate(unsigned int milliseconds);

  /*! \see process_kill */
  Process::Error kill(unsigned int milliseconds);

  /*! \see process_exit_status */
  Process::Error exit_status(int *exit_status);

  /*! \see process_system_error */
  static unsigned int system_error();

  static std::string error_to_string(Process::Error error);

private:
  struct process *process;
};

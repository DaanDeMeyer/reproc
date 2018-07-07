#pragma once

#include <string>
#include <vector>

class Process
{
public:
  static const unsigned int INFINITE;
  static const unsigned int PATH_MAX;

  // Error is a class instead of enum so we can assign its values to the values
  // of PROCESS_LIB_ERROR in the .cpp file which isn't possible with an enum.
  // Only the no-arg constructor is available which defaults to Process::SUCCESS
  class Error
  {
  public:
    Error();

    operator bool() const;

  private:
    Error(int value);
    int value_;

    // Make Process friend so we can instantiate Error constants inside Process
    // but outside of Error
    friend class Process;
    // Make friend so we can access value for comparison. Not equal doesn't need
    // to be friend since it can be implemented with the equality operator
    friend inline bool operator==(const Process::Error &lhs,
                                  const Process::Error &rhs);
  };

  static const Error SUCCESS;
  static const Error UNKNOWN_ERROR;
  static const Error WAIT_TIMEOUT;
  static const Error STREAM_CLOSED;
  static const Error STILL_RUNNING;
  static const Error MEMORY_ERROR;
  static const Error PIPE_LIMIT_REACHED;
  static const Error INTERRUPTED;
  static const Error IO_ERROR;
  static const Error PROCESS_LIMIT_REACHED;
  static const Error INVALID_UNICODE;
  static const Error PERMISSION_DENIED;
  static const Error SYMLINK_LOOP;
  static const Error FILE_NOT_FOUND;
  static const Error NAME_TOO_LONG;

  Process();
  ~Process();

  /* Enforce unique ownership */
  Process(const Process &) = delete;
  Process(Process &&other) noexcept;

  Process &operator=(const Process &) = delete;
  Process &operator=(Process &&other) noexcept;

  /*! /see process_start */
  Process::Error start(int argc, const char *argv[],
                       const char *working_directory);

  /*!
  Overload for convenient usage from C++.

  args has the same restrictions as argv in /see process_start except that the
  last element does not have to be null.
  */
  Process::Error start(const std::vector<std::string> &args,
                       const std::string *working_directory);

  /*! /see process_write */
  Process::Error write(const void *buffer, unsigned int to_write,
                       unsigned int *actual);

  /*! /see process_close_stdin */
  Process::Error close_stdin();

  /*! /see process_read */
  Process::Error read(void *buffer, unsigned int to_read, unsigned int *actual);

  /*! /see process_read_stderr */
  Process::Error read_stderr(void *buffer, unsigned int to_read,
                             unsigned int *actual);

  /*! /see process_wait */
  Process::Error wait(unsigned int milliseconds);

  /*! /see process_terminate */
  Process::Error terminate(unsigned int milliseconds);

  /*! /see process_kill */
  Process::Error kill(unsigned int milliseconds);

  /*! /see process_exit_status */
  Process::Error exit_status(int *exit_status);

  /*! /see process_system_error */
  unsigned int system_error();

private:
  struct process *process;
};

inline bool operator==(const Process::Error &lhs, const Process::Error &rhs);
inline bool operator!=(const Process::Error &lhs, const Process::Error &rhs);

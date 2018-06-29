#pragma once

#include <cstdint>

class Process
{
public:
  static const unsigned int INFINITE = 0xFFFFFFFF;

  enum Error {
    SUCCESS = 0,
    UNKNOWN_ERROR = -1,
    WAIT_TIMEOUT = -2,
    STREAM_CLOSED = -3,
    STILL_RUNNING = -4,
    MEMORY_ERROR = -5,
    PIPE_LIMIT_REACHED = -6,
    INTERRUPTED = -7,
    IO_ERROR = -8,
    PROCESS_LIMIT_REACHED = -9,
    INVALID_UNICODE = -10
  };

  Process();
  ~Process();

  Process(const Process &) = delete;
  Process(Process &&);

  Process &operator=(const Process &) = delete;
  Process &operator=(Process &&);

  /*! /see process_start */
  Process::Error start(const char *argv[], int argc,
                       const char *working_directory);

  /*! /see process_write */
  Process::Error write(const void *buffer, unsigned int to_write,
                       unsigned int *actual);

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
  long long system_error();

private:
  struct process *process;
};

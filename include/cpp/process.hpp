#pragma once

#include <cstdint>

class Process
{
public:
  static const uint32_t INFINITE = 0xFFFFFFFF;

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
  Process::Error write(const void *buffer, uint32_t to_write, uint32_t *actual);

  /*! /see process_read */
  Process::Error read(void *buffer, uint32_t to_read, uint32_t *actual);

  /*! /see process_read_stderr */
  Process::Error read_stderr(void *buffer, uint32_t to_read, uint32_t *actual);

  /*! /see process_wait */
  Process::Error wait(uint32_t milliseconds);

  /*! /see process_terminate */
  Process::Error terminate(uint32_t milliseconds);

  /*! /see process_kill */
  Process::Error kill(uint32_t milliseconds);

  /*! /see process_exit_status */
  Process::Error exit_status(int32_t *exit_status);

  /*! /see process_system_error */
  int64_t system_error();

private:
  struct process *process;
};

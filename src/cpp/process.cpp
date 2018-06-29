#include "process.hpp"

#include <process.h>
#include <stdexcept>

Process::Process() : process(nullptr)
{
  PROCESS_LIB_ERROR error = process_init(&process);
  if (error) { throw std::bad_alloc(); }
}

Process::~Process()
{
  if (process) { process_free(&process); }
}

Process::Process(Process &&other) : process(other.process)
{
  other.process = nullptr;
}

Process &Process::operator=(Process &&other)
{
  process = other.process;
  other.process = nullptr;
  return *this;
}

Process::Error Process::start(const char *argv[], int argc,
                              const char *working_directory)
{
  return static_cast<Error>(
      process_start(process, argv, argc, working_directory));
}

Process::Error Process::write(const void *buffer, uint32_t to_write,
                              uint32_t *actual)
{
  return static_cast<Error>(process_write(process, buffer, to_write, actual));
}

Process::Error Process::read(void *buffer, uint32_t to_read, uint32_t *actual)
{
  return static_cast<Error>(process_read(process, buffer, to_read, actual));
}

Process::Error Process::read_stderr(void *buffer, uint32_t to_read,
                                    uint32_t *actual)
{
  return static_cast<Error>(
      process_read_stderr(process, buffer, to_read, actual));
}

Process::Error Process::wait(uint32_t milliseconds)
{
  return static_cast<Error>(process_wait(process, milliseconds));
}

Process::Error Process::terminate(uint32_t milliseconds)
{
  return static_cast<Error>(process_terminate(process, milliseconds));
}

Process::Error Process::kill(uint32_t milliseconds)
{
  return static_cast<Error>(process_kill(process, milliseconds));
}

Process::Error Process::exit_status(int32_t *exit_status)
{
  return static_cast<Error>(process_exit_status(process, exit_status));
}

int64_t Process::system_error() { return process_system_error(); }

#include "process.hpp"

#include <cstdlib>
#include <new>
#include <process.h>

const unsigned int Process::INFINITE = PROCESS_LIB_INFINITE;

Process::Process()
{
  process = static_cast<process_type *>(malloc(process_size()));
  if (!process) { throw std::bad_alloc(); }
  process_init(process);
}

Process::~Process()
{
  if (process) {
    process_destroy(process);
    free(process);
  }
}

Process::Process(Process &&other) noexcept : process(other.process)
{
  other.process = nullptr;
}

Process &Process::operator=(Process &&other) noexcept
{
  process = other.process;
  other.process = nullptr;
  return *this;
}

Process::Error Process::start(int argc, const char *argv[],
                              const char *working_directory)
{
  return (Process::Error) process_start(process, argc, argv, working_directory);
}

Process::Error Process::start(const std::vector<std::string> &args,
                              const std::string *working_directory)
{
  const char **argv = new const char *[args.size() + 1];

  for (std::size_t i = 0; i < args.size(); i++) {
    argv[i] = args[i].c_str();
  }
  argv[args.size()] = nullptr;

  int argc = static_cast<int>(args.size());

  Error error = start(argc, argv,
                      working_directory ? working_directory->c_str() : nullptr);
  delete[] argv;

  return error;
}

Process::Error Process::close_stdin()
{
  return (Process::Error) process_close_stdin(process);
}

Process::Error Process::write(const void *buffer, unsigned int to_write,
                              unsigned int *actual)
{
  return (Process::Error) process_write(process, buffer, to_write, actual);
}

Process::Error Process::read(void *buffer, unsigned int to_read,
                             unsigned int *actual)
{
  return (Process::Error) process_read(process, buffer, to_read, actual);
}

Process::Error Process::read_stderr(void *buffer, unsigned int to_read,
                                    unsigned int *actual)
{
  return (Process::Error) process_read_stderr(process, buffer, to_read, actual);
}

Process::Error Process::wait(unsigned int milliseconds)
{
  return (Process::Error) process_wait(process, milliseconds);
}

Process::Error Process::terminate(unsigned int milliseconds)
{
  return (Process::Error) process_terminate(process, milliseconds);
}

Process::Error Process::kill(unsigned int milliseconds)
{
  return (Process::Error) process_kill(process, milliseconds);
}

Process::Error Process::exit_status(int *exit_status)
{
  return (Process::Error) process_exit_status(process, exit_status);
}

unsigned int Process::system_error() { return process_system_error(); }

Process::Error Process::system_error_string(char **error_string)
{
  return (Process::Error) process_system_error_string(error_string);
}

void Process::system_error_string_free(char *error_string)
{
  return process_system_error_string_free(error_string);
}

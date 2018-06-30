#include "process.hpp"

#include <cstdlib>
#include <new>
#include <process.h>

Process::Process()
{
  process = (process_type *) malloc(process_size());
  if (!process) { throw std::bad_alloc(); }
  PROCESS_LIB_ERROR error = process_init(process);
}

Process::~Process()
{
  if (process) {
    process_free(process);
    free(process);
  }
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

Process::Error Process::start(const std::vector<std::string> &args,
                              const std::string *working_directory)
{
  const char **argv = new const char *[args.size() + 1];

  for (int i = 0; i < args.size(); i++) {
    argv[i] = args[i].c_str();
  }
  argv[args.size()] = nullptr;

  int argc = static_cast<int>(args.size());

  Error error = start(argv, argc,
                      working_directory ? working_directory->c_str() : nullptr);
  free(argv);

  return error;
}

Process::Error Process::close_stdin()
{
  return static_cast<Error>(process_close_stdin(process));
}

Process::Error Process::write(const void *buffer, unsigned int to_write,
                              unsigned int *actual)
{
  return static_cast<Error>(process_write(process, buffer, to_write, actual));
}

Process::Error Process::read(void *buffer, unsigned int to_read,
                             unsigned int *actual)
{
  return static_cast<Error>(process_read(process, buffer, to_read, actual));
}

Process::Error Process::read_stderr(void *buffer, unsigned int to_read,
                                    unsigned int *actual)
{
  return static_cast<Error>(
      process_read_stderr(process, buffer, to_read, actual));
}

Process::Error Process::wait(unsigned int milliseconds)
{
  return static_cast<Error>(process_wait(process, milliseconds));
}

Process::Error Process::terminate(unsigned int milliseconds)
{
  return static_cast<Error>(process_terminate(process, milliseconds));
}

Process::Error Process::kill(unsigned int milliseconds)
{
  return static_cast<Error>(process_kill(process, milliseconds));
}

Process::Error Process::exit_status(int *exit_status)
{
  return static_cast<Error>(process_exit_status(process, exit_status));
}

unsigned int Process::system_error() { return process_system_error(); }

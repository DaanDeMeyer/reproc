#include "process-lib/process.hpp"

#include <array>
#include <cstdlib>
#include <new>
#include <ostream>

namespace process_lib
{

#include <process-lib/process.h>

const unsigned int Process::INFINITE = PROCESS_LIB_INFINITE;

Process::Process()
{
  process = static_cast<process_type *>(malloc(process_size()));
  if (!process) { throw std::bad_alloc(); }
  process_init(process);
}

Process::~Process()
{
  // Check for null because the object could have been moved
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
  return static_cast<Process::Error>(
      process_start(process, argc, argv, working_directory));
}

Process::Error Process::start(const std::vector<std::string> &args,
                              const std::string *working_directory)
{
  // Turn args into array of C strings
  auto argv = new const char *[args.size() + 1];
  for (std::size_t i = 0; i < args.size(); i++) {
    argv[i] = args[i].c_str();
  }
  argv[args.size()] = nullptr;

  // We don't expect so many args that the int will not be sufficient to count
  // them
  auto argc = static_cast<int>(args.size());

  Error error = start(argc, argv,
                      working_directory ? working_directory->c_str() : nullptr);
  delete[] argv;

  return error;
}

Process::Error Process::close_stdin()
{
  return static_cast<Process::Error>(process_close_stdin(process));
}

Process::Error Process::write(const void *buffer, unsigned int to_write,
                              unsigned int *bytes_written)
{
  return static_cast<Process::Error>(
      process_write(process, buffer, to_write, bytes_written));
}

Process::Error Process::read(void *buffer, unsigned int size,
                             unsigned int *bytes_read)
{
  return static_cast<Process::Error>(
      process_read(process, buffer, size, bytes_read));
}

Process::Error Process::read_stderr(void *buffer, unsigned int size,
                                    unsigned int *bytes_read)
{
  return static_cast<Process::Error>(
      process_read_stderr(process, buffer, size, bytes_read));
}

Process::Error Process::read_all(std::ostream &out)
{
  return read_all_generic([&out](const char *buffer, unsigned int size) {
    out.write(buffer, size);
  });
}

Process::Error Process::read_all_stderr(std::ostream &out)
{
  return read_all_stderr_generic([&out](const char *buffer, unsigned int size) {
    out.write(buffer, size);
  });
}

Process::Error Process::read_all(std::string &out)
{
  return read_all_generic([&out](const char *buffer, unsigned int size) {
    out.append(buffer, size);
  });
}

Process::Error Process::read_all_stderr(std::string &out)
{
  return read_all_stderr_generic([&out](const char *buffer, unsigned int size) {
    out.append(buffer, size);
  });
}

Process::Error Process::wait(unsigned int milliseconds)
{
  return static_cast<Process::Error>(process_wait(process, milliseconds));
}

Process::Error Process::terminate(unsigned int milliseconds)
{
  return static_cast<Process::Error>(process_terminate(process, milliseconds));
}

Process::Error Process::kill(unsigned int milliseconds)
{
  return static_cast<Process::Error>(process_kill(process, milliseconds));
}

Process::Error Process::exit_status(int *exit_status)
{
  return static_cast<Process::Error>(process_exit_status(process, exit_status));
}

unsigned int Process::system_error() { return process_system_error(); }

std::string Process::error_to_string(Process::Error error)
{
  if (error == Process::UNKNOWN_ERROR) {
    return "process-lib => unknown error. system error = " +
           std::to_string(system_error());
  }
  return process_error_to_string(static_cast<PROCESS_LIB_ERROR>(error));
}

} // namespace process_lib

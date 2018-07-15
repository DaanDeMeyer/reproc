#include "process-lib/process.hpp"

#include <process-lib/process.h>

#include <array>
#include <cstdlib>
#include <functional>
#include <new>
#include <ostream>

namespace
{

using read_f =
    std::function<Process::Error(void *, unsigned int, unsigned int *)>;

Process::Error read_all(std::ostream &out, const read_f &read_some)
{
  std::array<char, 1024> buffer{};
  auto buffer_size = static_cast<unsigned int>(buffer.size() - 1);

  Process::Error error = Process::SUCCESS;

  while (true) {
    unsigned int bytes_read = 0;
    error = read_some(buffer.data(), buffer_size, &bytes_read);
    if (error) { break; }

    buffer[bytes_read] = '\0';
    out << buffer.data();
  }

  if (error != Process::STREAM_CLOSED) { return error; }

  return Process::SUCCESS;
}

} // namespace

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
  auto read_some = std::bind(&Process::read, this, std::placeholders::_1,
                             std::placeholders::_2, std::placeholders::_3);

  return ::read_all(out, read_some);
}

Process::Error Process::read_all_stderr(std::ostream &out)
{
  auto read_some = std::bind(&Process::read_stderr, this, std::placeholders::_1,
                             std::placeholders::_2, std::placeholders::_3);

  return ::read_all(out, read_some);
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

Process::Error Process::system_error_string(char **error_string)
{
  return static_cast<Process::Error>(process_system_error_string(error_string));
}

void Process::system_error_string_free(char *error_string)
{
  return process_system_error_string_free(error_string);
}

#include "process.hpp"

#include <cstdlib>
#include <new>
#include <process.h>

const unsigned int Process::INFINITE = PROCESS_LIB_INFINITE;

Process::Error::Error() : value_(PROCESS_LIB_SUCCESS) {}
Process::Error::Error(int value) : value_(value) {}

Process::Error::operator bool() const { return *this != Process::SUCCESS; }

inline bool operator==(const Process::Error &lhs, const Process::Error &rhs)
{
  return lhs.value_ == rhs.value_;
}
inline bool operator!=(const Process::Error &lhs, const Process::Error &rhs)
{
  return !(lhs == rhs);
}

// clang-format off
const Process::Error Process::SUCCESS = PROCESS_LIB_SUCCESS;
const Process::Error Process::UNKNOWN_ERROR = PROCESS_LIB_UNKNOWN_ERROR;
const Process::Error Process::WAIT_TIMEOUT = PROCESS_LIB_WAIT_TIMEOUT;
const Process::Error Process::STREAM_CLOSED = PROCESS_LIB_STREAM_CLOSED;
const Process::Error Process::STILL_RUNNING = PROCESS_LIB_STILL_RUNNING;
const Process::Error Process::MEMORY_ERROR = PROCESS_LIB_MEMORY_ERROR;
const Process::Error Process::PIPE_LIMIT_REACHED = PROCESS_LIB_PIPE_LIMIT_REACHED;
const Process::Error Process::INTERRUPTED = PROCESS_LIB_INTERRUPTED;
const Process::Error Process::IO_ERROR = PROCESS_LIB_IO_ERROR;
const Process::Error Process::PROCESS_LIMIT_REACHED = PROCESS_LIB_PROCESS_LIMIT_REACHED;
const Process::Error Process::INVALID_UNICODE = PROCESS_LIB_INVALID_UNICODE;
const Process::Error Process::PERMISSION_DENIED = PROCESS_LIB_PERMISSION_DENIED;
const Process::Error Process::SYMLINK_LOOP = PROCESS_LIB_SYMLINK_LOOP;
const Process::Error Process::FILE_NOT_FOUND = PROCESS_LIB_FILE_NOT_FOUND;
const Process::Error Process::NAME_TOO_LONG = PROCESS_LIB_NAME_TOO_LONG;
// clang-format on

Process::Process()
{
  process = static_cast<process_type *>(malloc(process_size()));
  if (process) { throw std::bad_alloc(); }
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
  return process_start(process, argc, argv, working_directory);
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

Process::Error Process::close_stdin() { return process_close_stdin(process); }

Process::Error Process::write(const void *buffer, unsigned int to_write,
                              unsigned int *actual)
{
  return process_write(process, buffer, to_write, actual);
}

Process::Error Process::read(void *buffer, unsigned int to_read,
                             unsigned int *actual)
{
  return process_read(process, buffer, to_read, actual);
}

Process::Error Process::read_stderr(void *buffer, unsigned int to_read,
                                    unsigned int *actual)
{
  return process_read_stderr(process, buffer, to_read, actual);
}

Process::Error Process::wait(unsigned int milliseconds)
{
  return process_wait(process, milliseconds);
}

Process::Error Process::terminate(unsigned int milliseconds)
{
  return process_terminate(process, milliseconds);
}

Process::Error Process::kill(unsigned int milliseconds)
{
  return process_kill(process, milliseconds);
}

Process::Error Process::exit_status(int *exit_status)
{
  return process_exit_status(process, exit_status);
}

unsigned int Process::system_error() { return process_system_error(); }

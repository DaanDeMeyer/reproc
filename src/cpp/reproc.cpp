#include "reproc/reproc.hpp"

namespace reproc {

#include <reproc/reproc.h>

const unsigned int infinite = REPROC_INFINITE;

process::process() : process_(new struct reproc_type()) {}

process::~process()
{
  // Check for null because the object could have been moved
  if (process_) { reproc_destroy(process_.get()); }
}

reproc::error process::start(int argc, const char *const *argv,
                             const char *working_directory)
{
  return static_cast<reproc::error>(
      reproc_start(process_.get(), argc, argv, working_directory));
}

reproc::error process::start(const std::vector<std::string> &args,
                             const std::string *working_directory)
{
  // Turn args into array of C strings
  auto argv = std::vector<const char *>(args.size() + 1);

  for (std::size_t i = 0; i < args.size(); i++) {
    argv[i] = args[i].c_str();
  }
  argv[args.size()] = nullptr;

  // We don't expect so many args that an int will insufficient to count them
  auto argc = static_cast<int>(args.size());

  reproc::error error = start(argc, &argv[0] /* std::vector -> C array */,
                              working_directory ? working_directory->c_str()
                                                : nullptr);

  return error;
}

void process::close(reproc::stream stream)
{
  return reproc_close(process_.get(), static_cast<REPROC_STREAM>(stream));
}

reproc::error process::write(const void *buffer, unsigned int to_write,
                             unsigned int *bytes_written)
{
  return static_cast<reproc::error>(
      reproc_write(process_.get(), buffer, to_write, bytes_written));
}

reproc::error process::read(reproc::stream stream, void *buffer,
                            unsigned int size, unsigned int *bytes_read)
{
  return static_cast<reproc::error>(
      reproc_read(process_.get(), static_cast<REPROC_STREAM>(stream), buffer,
                  size, bytes_read));
}

reproc::error process::wait(unsigned int milliseconds,
                            unsigned int *exit_status)
{
  return static_cast<reproc::error>(
      reproc_wait(process_.get(), milliseconds, exit_status));
}

reproc::error process::terminate(unsigned int milliseconds)
{
  return static_cast<reproc::error>(
      reproc_terminate(process_.get(), milliseconds));
}

reproc::error process::kill(unsigned int milliseconds)
{
  return static_cast<reproc::error>(reproc_kill(process_.get(), milliseconds));
}

} // namespace reproc

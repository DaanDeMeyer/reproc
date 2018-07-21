#include "reproc/reproc.hpp"

#include <ostream>

namespace reproc {

#include <reproc/reproc.h>

const unsigned int INFINITE = REPROC_INFINITE;

Reproc::Reproc() : reproc(new struct reproc_type()) {}

Reproc::~Reproc()
{
  // Check for null because the object could have been moved
  if (reproc) { reproc_destroy(reproc.get()); }
}

Error Reproc::start(int argc, const char *const *argv,
                    const char *working_directory)
{
  return static_cast<Error>(
      reproc_start(reproc.get(), argc, argv, working_directory));
}

Error Reproc::start(const std::vector<std::string> &args,
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

  Error error = start(argc, &argv[0] /* std::vector -> C array */,
                      working_directory ? working_directory->c_str() : nullptr);

  return error;
}

Error Reproc::close(Stream stream)
{
  return static_cast<Error>(
      reproc_close(reproc.get(), static_cast<REPROC_STREAM>(stream)));
}

Error Reproc::write(const void *buffer, unsigned int to_write,
                    unsigned int *bytes_written)
{
  return static_cast<Error>(
      reproc_write(reproc.get(), buffer, to_write, bytes_written));
}

Error Reproc::read(Stream stream, void *buffer, unsigned int size,
                   unsigned int *bytes_read)
{
  return static_cast<Error>(reproc_read(reproc.get(),
                                        static_cast<REPROC_STREAM>(stream),
                                        buffer, size, bytes_read));
}

Error Reproc::wait(unsigned int milliseconds)
{
  return static_cast<Error>(reproc_wait(reproc.get(), milliseconds));
}

Error Reproc::terminate(unsigned int milliseconds)
{
  return static_cast<Error>(reproc_terminate(reproc.get(), milliseconds));
}

Error Reproc::kill(unsigned int milliseconds)
{
  return static_cast<Error>(reproc_kill(reproc.get(), milliseconds));
}

Error Reproc::exit_status(int *exit_status)
{
  return static_cast<Error>(reproc_exit_status(reproc.get(), exit_status));
}

} // namespace reproc

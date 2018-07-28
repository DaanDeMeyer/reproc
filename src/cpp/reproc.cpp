#include "reproc/reproc.hpp"

namespace reproc
{

#include <reproc/reproc.h>

static std::error_code reproc_error_to_error_code(REPROC_ERROR error)
{
  switch (error) {
  case REPROC_SUCCESS: return {};
  case REPROC_WAIT_TIMEOUT: return { error, reproc::error_category() };
  case REPROC_STREAM_CLOSED: return { error, reproc::error_category() };
  case REPROC_PARTIAL_WRITE: return { error, reproc::error_category() };
  default:
  // errno values should have generic_category() but windows errors should have
  // system_category()
#ifdef _WIN32
    return { static_cast<int>(reproc_system_error()), std::system_category() };
#else
    return { static_cast<int>(reproc_system_error()), std::generic_category() };
#endif
  }
}

const unsigned int infinite = REPROC_INFINITE;

process::process() : process_(new struct reproc_type()) {}

process::~process() noexcept
{
  // Check for null because the object could have been moved from
  if (process_) { reproc_destroy(process_.get()); }
}

std::error_code process::start(int argc, const char *const *argv,
                               const char *working_directory) noexcept
{
  REPROC_ERROR error = reproc_start(process_.get(), argc, argv,
                                    working_directory);

  return reproc_error_to_error_code(error);
}

std::error_code process::start(const std::vector<std::string> &args,
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

  std::error_code error = start(argc, &argv[0] /* std::vector -> C array */,
                                working_directory ? working_directory->c_str()
                                                  : nullptr);

  return error;
}

void process::close(reproc::stream stream) noexcept
{
  return reproc_close(process_.get(), static_cast<REPROC_STREAM>(stream));
}

std::error_code process::write(const void *buffer, unsigned int to_write,
                               unsigned int *bytes_written) noexcept
{
  REPROC_ERROR error = reproc_write(process_.get(), buffer, to_write,
                                    bytes_written);
  return reproc_error_to_error_code(error);
}

std::error_code process::read(reproc::stream stream, void *buffer,
                              unsigned int size,
                              unsigned int *bytes_read) noexcept
{
  REPROC_ERROR error = reproc_read(process_.get(),
                                   static_cast<REPROC_STREAM>(stream), buffer,
                                   size, bytes_read);
  return reproc_error_to_error_code(error);
}

std::error_code process::wait(unsigned int milliseconds,
                              unsigned int *exit_status) noexcept
{
  REPROC_ERROR error = reproc_wait(process_.get(), milliseconds, exit_status);
  return reproc_error_to_error_code(error);
}

std::error_code process::terminate(unsigned int milliseconds) noexcept
{
  REPROC_ERROR error = reproc_terminate(process_.get(), milliseconds);
  return reproc_error_to_error_code(error);
}

std::error_code process::kill(unsigned int milliseconds) noexcept
{
  REPROC_ERROR error = reproc_kill(process_.get(), milliseconds);
  return reproc_error_to_error_code(error);
}

} // namespace reproc

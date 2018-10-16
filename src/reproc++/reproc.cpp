#include "reproc++/reproc.hpp"

#include "reproc/reproc.h"

static std::error_code reproc_error_to_error_code(REPROC_ERROR error)
{
  switch (error) {
  case REPROC_SUCCESS: return {};
  // The following three errors are reproc specific and don't have a
  // corresponding OS error. Instead, we represent them through an
  // std::error_code with the same value in the reproc error category.
  case REPROC_WAIT_TIMEOUT:
  case REPROC_STREAM_CLOSED:
  case REPROC_PARTIAL_WRITE: return { error, reproc::error_category() };
  default:
    // errno values belong to the generic category. However, on Windows we get
    // Windows specific errors (using GetLastError() instead of errno) which
    // belong to the system category.
#ifdef _WIN32
    return { static_cast<int>(reproc_system_error()), std::system_category() };
#else
    return { static_cast<int>(reproc_system_error()), std::generic_category() };
#endif
  }
}

namespace reproc
{

reproc::cleanup operator|(reproc::cleanup lhs, reproc::cleanup rhs) noexcept
{
  return static_cast<reproc::cleanup>(static_cast<int>(lhs) |
                                      static_cast<int>(rhs));
}

process::process(reproc::cleanup cleanup_flags, unsigned int timeout)
    : cleanup_flags_(cleanup_flags), timeout_(timeout),
      process_(new reproc_type()), running_(false)
{
}

process::~process() noexcept
{
  if (process_ && running_) { stop(cleanup_flags_, timeout_, nullptr); }
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
  // Turn args into array of C strings.
  auto argv = std::vector<const char *>(args.size() + 1);

  for (std::size_t i = 0; i < args.size(); i++) {
    argv[i] = args[i].c_str();
  }
  argv[args.size()] = nullptr;

  // We don't expect so many args that an int will insufficient to count them.
  auto argc = static_cast<int>(args.size());

  std::error_code error = start(argc, &argv[0] /* std::vector -> C array. */,
                                working_directory ? working_directory->c_str()
                                                  : nullptr);

  if (!error) { running_ = true; }

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

std::error_code process::stop(reproc::cleanup cleanup_flags,
                              unsigned int timeout,
                              unsigned int *exit_status) noexcept
{
  REPROC_ERROR error = reproc_stop(process_.get(),
                                   static_cast<int>(cleanup_flags), timeout,
                                   exit_status);

  running_ = false;

  return reproc_error_to_error_code(error);
}

} // namespace reproc

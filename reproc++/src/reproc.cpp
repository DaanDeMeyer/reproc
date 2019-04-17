#include <reproc++/reproc.hpp>

#include <reproc/reproc.h>

#include <array>

static std::error_code reproc_error_to_error_code(REPROC_ERROR error)
{
  switch (error) {
  case REPROC_SUCCESS:
    return {};
  // The following three errors are reproc specific and don't have a
  // corresponding OS error. Instead, we represent them through an
  // `std::error_code` with the same value as `error` in the reproc error
  // category.
  case REPROC_WAIT_TIMEOUT:
  case REPROC_STREAM_CLOSED:
  case REPROC_PARTIAL_WRITE:
    return { error, reproc::error_category() };
  default:
    // `errno` values belong to the generic category. However, on Windows we get
    // Windows specific errors which belong to the system category.
#ifdef _WIN32
    return { static_cast<int>(reproc_system_error()), std::system_category() };
#else
    return { static_cast<int>(reproc_system_error()), std::generic_category() };
#endif
  }
}

namespace reproc
{

const reproc::milliseconds infinite = reproc::milliseconds(0xFFFFFFFF);

process::process(cleanup c1,
                 reproc::milliseconds t1,
                 cleanup c2,
                 reproc::milliseconds t2,
                 cleanup c3,
                 reproc::milliseconds t3)
    : process_(new reproc_type()),
      c1_(c1),
      t1_(t1),
      c2_(c2),
      t2_(t2),
      c3_(c3),
      t3_(t3)
{
}

process::~process() noexcept
{
  // No cleanup is required if the object has been moved from.
  if (!process_) {
    return;
  }

  stop(c1_, t1_, c2_, t2_, c3_, t3_);

  reproc_destroy(process_.get());
}

process::process(process &&) noexcept = default; // NOLINT
process &process::operator=(process &&) noexcept = default;

std::error_code process::start(int argc,
                               const char *const *argv,
                               const char *working_directory) noexcept
{
  REPROC_ERROR error = reproc_start(process_.get(), argc, argv,
                                    working_directory);
  return reproc_error_to_error_code(error);
}

std::error_code process::read(reproc::stream stream,
                              void *buffer,
                              unsigned int size,
                              unsigned int *bytes_read) noexcept
{
  REPROC_ERROR error = reproc_read(process_.get(),
                                   static_cast<REPROC_STREAM>(stream), buffer,
                                   size, bytes_read);
  return reproc_error_to_error_code(error);
}

std::error_code process::write(const void *buffer,
                               unsigned int to_write,
                               unsigned int *bytes_written) noexcept
{
  REPROC_ERROR error = reproc_write(process_.get(), buffer, to_write,
                                    bytes_written);
  return reproc_error_to_error_code(error);
}

void process::close(reproc::stream stream) noexcept
{
  return reproc_close(process_.get(), static_cast<REPROC_STREAM>(stream));
}

bool process::running() noexcept { return reproc_running(process_.get()); }

std::error_code process::wait(reproc::milliseconds timeout) noexcept
{
  REPROC_ERROR error = reproc_wait(process_.get(), timeout.count());
  return reproc_error_to_error_code(error);
}

std::error_code process::terminate() noexcept
{
  REPROC_ERROR error = reproc_terminate(process_.get());
  return reproc_error_to_error_code(error);
}

std::error_code process::kill() noexcept
{
  REPROC_ERROR error = reproc_kill(process_.get());
  return reproc_error_to_error_code(error);
}

std::error_code process::stop(cleanup c1,
                              reproc::milliseconds t1,
                              cleanup c2,
                              reproc::milliseconds t2,
                              cleanup c3,
                              reproc::milliseconds t3) noexcept
{
  REPROC_ERROR error = reproc_stop(process_.get(),
                                   static_cast<REPROC_CLEANUP>(c1), t1.count(),
                                   static_cast<REPROC_CLEANUP>(c2), t2.count(),
                                   static_cast<REPROC_CLEANUP>(c3), t3.count());
  return reproc_error_to_error_code(error);
}

unsigned int process::exit_status() noexcept
{
  return reproc_exit_status(process_.get());
}

} // namespace reproc

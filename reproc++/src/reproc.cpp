#include <reproc++/reproc.hpp>

#include <reproc/reproc.h>

#include <array>
#include <cassert>

static std::error_code error_to_error_code(REPROC_ERROR error)
{
  switch (error) {
    case REPROC_SUCCESS:
    case REPROC_ERROR_WAIT_TIMEOUT:
    case REPROC_ERROR_STREAM_CLOSED:
      return static_cast<reproc::error>(error);
    case REPROC_ERROR_SYSTEM:
      // Convert operating system errors back to platform-specific error codes
      // to preserve the original error value and message. These can then be
      // matched against using the `std::errc` error condition.
      return { static_cast<int>(reproc_error_system()),
               std::system_category() };
  }

  assert(false);
  return {};
}

namespace reproc {

const milliseconds infinite = milliseconds(0xFFFFFFFF);

process::process(cleanup c1,
                 milliseconds t1,
                 cleanup c2,
                 milliseconds t2,
                 cleanup c3,
                 milliseconds t3)
    : process_(reproc_new(), reproc_destroy),
      c1_(c1),
      t1_(t1),
      c2_(c2),
      t2_(t2),
      c3_(c3),
      t3_(t3)
{}

process::~process() noexcept
{
  // No cleanup is required if the object has been moved from.
  if (!process_) {
    return;
  }

  stop(c1_, t1_, c2_, t2_, c3_, t3_);
}

process::process(process &&) noexcept = default; // NOLINT
process &process::operator=(process &&) noexcept = default;

std::error_code process::start(const arguments &arguments,
                               const options &options) noexcept
{
  reproc_options reproc_options = {
    options.environment.data(),
    options.working_directory,
    { static_cast<REPROC_REDIRECT>(options.redirect.in),
      static_cast<REPROC_REDIRECT>(options.redirect.out),
      static_cast<REPROC_REDIRECT>(options.redirect.err) }
  };

  REPROC_ERROR error = reproc_start(process_.get(), arguments.data(),
                                    reproc_options);
  return error_to_error_code(error);
}

std::error_code process::read(stream *stream,
                              uint8_t *buffer,
                              unsigned int size,
                              unsigned int *bytes_read) noexcept
{
  REPROC_STREAM tmp = {};
  REPROC_ERROR error = reproc_read(process_.get(), &tmp, buffer, size,
                                   bytes_read);

  if (stream != nullptr) {
    *stream = static_cast<reproc::stream>(tmp);
  }

  return error_to_error_code(error);
}

std::error_code process::write(const uint8_t *buffer,
                               unsigned int size) noexcept
{
  REPROC_ERROR error = reproc_write(process_.get(), buffer, size);
  return error_to_error_code(error);
}

void process::close(stream stream) noexcept
{
  return reproc_close(process_.get(), static_cast<REPROC_STREAM>(stream));
}

bool process::running() noexcept
{
  return reproc_running(process_.get());
}

std::error_code process::wait(milliseconds timeout) noexcept
{
  REPROC_ERROR error = reproc_wait(process_.get(), timeout.count());
  return error_to_error_code(error);
}

std::error_code process::terminate() noexcept
{
  REPROC_ERROR error = reproc_terminate(process_.get());
  return error_to_error_code(error);
}

std::error_code process::kill() noexcept
{
  REPROC_ERROR error = reproc_kill(process_.get());
  return error_to_error_code(error);
}

std::error_code process::stop(cleanup c1,
                              milliseconds t1,
                              cleanup c2,
                              milliseconds t2,
                              cleanup c3,
                              milliseconds t3) noexcept
{
  REPROC_ERROR error = reproc_stop(process_.get(),
                                   static_cast<REPROC_CLEANUP>(c1), t1.count(),
                                   static_cast<REPROC_CLEANUP>(c2), t2.count(),
                                   static_cast<REPROC_CLEANUP>(c3), t3.count());
  return error_to_error_code(error);
}

int process::exit_status() noexcept
{
  return reproc_exit_status(process_.get());
}

} // namespace reproc

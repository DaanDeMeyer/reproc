#include <reproc++/reproc.hpp>

#include <reproc/reproc.h>

namespace reproc {

static std::error_code error_to_error_code(REPROC_ERROR error)
{
  switch (error) {
    case REPROC_SUCCESS:
    case REPROC_ERROR_WAIT_TIMEOUT:
    case REPROC_ERROR_STREAM_CLOSED:
      return static_cast<enum error>(error);
    case REPROC_ERROR_SYSTEM:
      // Convert operating system errors back to platform-specific error codes
      // to preserve the original error value and message. These can then be
      // matched against using the `std::errc` error condition.
      return { static_cast<int>(reproc_error_system()),
               std::system_category() };
  }

  return {};
}

static reproc_stop_actions
stop_actions_to_reproc(stop_actions stop_actions)
{
  return { { static_cast<REPROC_STOP>(stop_actions.first.action),
             stop_actions.first.timeout.count() },
           { static_cast<REPROC_STOP>(stop_actions.second.action),
             stop_actions.second.timeout.count() },
           { static_cast<REPROC_STOP>(stop_actions.third.action),
             stop_actions.third.timeout.count() } };
}

const milliseconds infinite = milliseconds(0xFFFFFFFF);

process::process() : process_(reproc_new(), reproc_destroy) {}

std::error_code process::start(const arguments &arguments,
                               const options &options) noexcept
{
  reproc_options reproc_options = {
    options.environment.data(),
    options.working_directory,
    { static_cast<REPROC_REDIRECT>(options.redirect.in),
      static_cast<REPROC_REDIRECT>(options.redirect.out),
      static_cast<REPROC_REDIRECT>(options.redirect.err) },
    stop_actions_to_reproc(options.stop_actions)
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
    *stream = static_cast<enum stream>(tmp);
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

std::error_code process::stop(stop_actions stop_actions) noexcept
{
  REPROC_ERROR error = reproc_stop(process_.get(),
                                   stop_actions_to_reproc(stop_actions));

  return error_to_error_code(error);
}

int process::exit_status() noexcept
{
  return reproc_exit_status(process_.get());
}

} // namespace reproc

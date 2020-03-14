#pragma once

#include <reproc++/reproc.hpp>

#include <mutex>
#include <ostream>
#include <string>

namespace reproc {

/*!
`reproc_drain` but takes lambdas as sinks and defaults to waiting indefinitely
for each read to complete.

`out` and `err` expect the following signature:

```c++
bool sink(stream stream, const uint8_t *buffer, size_t size);
```
*/
template <typename Out, typename Err>
std::error_code drain(process &process, Out &&out, Err &&err)
{
  static constexpr uint8_t initial = 0;

  // A single call to `read` might contain multiple messages. By always calling
  // both sinks once with no data before reading, we give them the chance to
  // process all previous output before reading from the child process again.
  if (!out(stream::in, &initial, 0) || !err(stream::in, &initial, 0)) {
    return {};
  }

  static constexpr size_t BUFFER_SIZE = 4096;
  uint8_t buffer[BUFFER_SIZE] = {};
  std::error_code ec;

  while (true) {
    int events = 0;
    std::tie(events, ec) = process.poll(event::out | event::err, infinite);
    if (ec) {
      break;
    }

    if (events & event::deadline) {
      ec = { static_cast<int>(std::errc::timed_out), std::generic_category() };
      break;
    }

    stream stream = events & event::out ? stream::out : stream::err;

    size_t bytes_read = 0;
    std::tie(bytes_read, ec) = process.read(stream, buffer, BUFFER_SIZE);
    if (ec && ec != error::broken_pipe) {
      break;
    }

    bytes_read = ec == error::broken_pipe ? 0 : bytes_read;
    auto &sink = stream == stream::out ? out : err;

    // `sink` returns false to tell us to stop reading.
    if (!sink(stream, buffer, bytes_read)) {
      break;
    }
  }

  return ec == error::broken_pipe ? std::error_code() : ec;
}

namespace sink {

/*! Reads all output into `string`. */
class string {
  std::string &string_;

public:
  explicit string(std::string &string) noexcept : string_(string) {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    (void) stream;
    string_.append(reinterpret_cast<const char *>(buffer), size);
    return true;
  }
};

/*! Forwards all output to `ostream`. */
class ostream {
  std::ostream &ostream_;

public:
  explicit ostream(std::ostream &ostream) noexcept : ostream_(ostream) {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    (void) stream;
    ostream_.write(reinterpret_cast<const char *>(buffer),
                   static_cast<std::streamsize>(size));
    return true;
  }
};

/*! Discards all output. */
class discard {
public:
  bool operator()(stream stream, const uint8_t *buffer, size_t size) const
      noexcept
  {
    (void) stream;
    (void) buffer;
    (void) size;

    return true;
  }
};

constexpr discard null = discard();

namespace thread_safe {

/*! `sink::string` but locks the given mutex before invoking the sink. */
class string {
  sink::string sink_;
  std::mutex &mutex_;

public:
  string(std::string &string, std::mutex &mutex) noexcept
      : sink_(string), mutex_(mutex)
  {}

  bool operator()(stream stream, const uint8_t *buffer, size_t size)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return sink_(stream, buffer, size);
  }
};

}

}
}

#include <doctest.h>

#include <reproc/reproc.h>

#include <array>
#include <string>

static void io(const char *mode,
               const std::string &input,
               const std::string &expected)
{
  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_string(error));

  reproc_t io;
  std::array<const char *, 3> argv = { RESOURCE_DIRECTORY "/io", mode,
                                       nullptr };

  error = reproc_start(&io, argv.data(), nullptr, nullptr);
  REQUIRE(!error);

  unsigned int size = static_cast<unsigned int>(input.size());

  unsigned int bytes_written = 0;
  error = reproc_write(&io, reinterpret_cast<const uint8_t *>(input.data()),
                       size, &bytes_written);
  REQUIRE(!error);

  reproc_close(&io, REPROC_STREAM_IN);

  std::string output;
  static constexpr unsigned int BUFFER_SIZE = 1024;
  std::array<uint8_t, BUFFER_SIZE> buffer = {};

  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(&io, nullptr, buffer.data(), BUFFER_SIZE, &bytes_read);
    if (error != REPROC_SUCCESS) {
      break;
    }

    output.append(reinterpret_cast<const char *>(buffer.data()), bytes_read);
  }

  REQUIRE_EQ(output, expected);

  error = reproc_wait(&io, REPROC_INFINITE);
  REQUIRE(!error);
  REQUIRE((reproc_exit_status(&io) == 0));

  reproc_destroy(&io);
}

TEST_CASE("io")
{
  std::string message = "reproc stands for REdirected PROCess";

  io("stdout", message, message);
  io("stderr", message, message);
  io("both", message, message + message);
}

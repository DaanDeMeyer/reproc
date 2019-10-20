#include <doctest.h>

#include <reproc/reproc.h>

#include <array>
#include <string>

static void io(const char *program, REPROC_STREAM stream)
{
  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_string(error));

  reproc_t io;
  std::array<const char *, 2> argv = { program, nullptr };

  error = reproc_start(&io, argv.data(), nullptr, nullptr);
  REQUIRE(!error);

  std::string message = "reproc stands for REdirected PROCess";
  unsigned int size = static_cast<unsigned int>(message.size());

  unsigned int bytes_written = 0;
  error = reproc_write(&io, reinterpret_cast<const uint8_t *>(message.data()),
                       size, &bytes_written);
  REQUIRE(!error);

  reproc_close(&io, REPROC_STREAM_IN);

  std::string output;
  static constexpr unsigned int BUFFER_SIZE = 1024;
  std::array<uint8_t, BUFFER_SIZE> buffer = {};

  while (true) {
    REPROC_STREAM actual = {};
    unsigned int bytes_read = 0;
    error = reproc_read(&io, &actual, buffer.data(), BUFFER_SIZE, &bytes_read);
    if (error != REPROC_SUCCESS) {
      break;
    }

    REQUIRE(actual == stream);
    output.append(reinterpret_cast<const char *>(buffer.data()), bytes_read);
  }

  REQUIRE_EQ(output, message);

  error = reproc_wait(&io, REPROC_INFINITE);
  REQUIRE(!error);
  REQUIRE((reproc_exit_status(&io) == 0));

  reproc_destroy(&io);
}

TEST_CASE("io")
{
  io(RESOURCE_DIRECTORY "/stdout", REPROC_STREAM_OUT);
  io(RESOURCE_DIRECTORY "/stderr", REPROC_STREAM_ERR);
}

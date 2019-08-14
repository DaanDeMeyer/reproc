#include <doctest.h>

#include <reproc/reproc.h>

#include <array>
#include <string>

void io(const char *program, REPROC_STREAM stream)
{
  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_strerror(error));

  reproc_t io;
  std::array<const char *, 2> argv = { program, nullptr };

  error = reproc_start(&io, argv.data(), nullptr);
  REQUIRE(!error);

  std::string message = "reproc stands for REdirected PROCess";
  auto size = static_cast<unsigned int>(message.length());

  unsigned int bytes_written = 0;
  error = reproc_write(&io, reinterpret_cast<const uint8_t *>(message.data()),
                       size, &bytes_written);
  REQUIRE(!error);

  reproc_close(&io, REPROC_STREAM_IN);

  std::string output;
  static constexpr unsigned int BUFFER_SIZE = 1024;
  std::array<uint8_t, BUFFER_SIZE> buffer = {};

  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(&io, stream, buffer.data(), BUFFER_SIZE, &bytes_read);
    if (error != REPROC_SUCCESS) {
      break;
    }

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
  io("reproc/resources/stdout", REPROC_STREAM_OUT);
  io("reproc/resources/stderr", REPROC_STREAM_ERR);
}

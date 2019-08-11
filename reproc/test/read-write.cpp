#include <doctest/doctest.h>
#include <reproc/reproc.h>

#include <array>
#include <string>

TEST_SUITE("reproc")
{
  TEST_CASE("read-write")
  {
    reproc_t io;

    int error = REPROC_SUCCESS;
    CAPTURE(error);

    static constexpr unsigned int BUFFER_SIZE = 1024;
    std::array<uint8_t, BUFFER_SIZE> buffer = {};

    std::string message;
    unsigned int message_length = 0;

    std::array<const char *, 2> argv = { nullptr, nullptr };

    REPROC_STREAM stream = REPROC_STREAM_IN;

    SUBCASE("stdout")
    {
      message = "This is stdout";
      message_length = static_cast<unsigned int>(message.length());

      argv[0] = "reproc/resources/stdout";

      stream = REPROC_STREAM_OUT;
    }

    SUBCASE("stderr")
    {
      message = "This is stderr";
      message_length = static_cast<unsigned int>(message.length());

      argv[0] = "reproc/resources/stderr";

      stream = REPROC_STREAM_ERR;
    }

    error = reproc_start(&io, argv.data(), nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, reinterpret_cast<const uint8_t *>(message.data()),
                         message_length, &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_STREAM_IN);

    std::string output{};

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
}

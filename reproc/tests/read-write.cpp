#include <doctest.h>
#include <reproc/reproc.h>

#include <array>
#include <string>

TEST_SUITE("reproc")
{
  TEST_CASE("read-write")
  {
    reproc_type io;

    int error = REPROC_SUCCESS;
    CAPTURE(error);

    static constexpr unsigned int BUFFER_SIZE = 1024;
    std::array<char, BUFFER_SIZE> buffer = { {} };

    std::string message;
    unsigned int message_length = 0;

    static constexpr unsigned int ARGV_SIZE = 2;
    std::array<const char *, ARGV_SIZE> argv = { { nullptr, nullptr } };

    REPROC_STREAM stream = REPROC_IN;

    SUBCASE("stdout")
    {
      message = "This is stdout";
      message_length = static_cast<unsigned int>(message.length());

      argv[0] = "reproc/resources/stdout";

      stream = REPROC_OUT;
    }

    SUBCASE("stderr")
    {
      message = "This is stderr";
      message_length = static_cast<unsigned int>(message.length());

      argv[0] = "reproc/resources/stderr";

      stream = REPROC_ERR;
    }

    error = reproc_start(&io, ARGV_SIZE - 1, argv.data(), nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, message.data(), message_length, &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_IN);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&io, stream, buffer.data(), BUFFER_SIZE, &bytes_read);
      if (error != REPROC_SUCCESS) {
        break;
      }

      output.append(buffer.data(), bytes_read);
    }

    REQUIRE_EQ(output, message);

    error = reproc_wait(&io, REPROC_INFINITE);
    REQUIRE(!error);
    REQUIRE((reproc_exit_status(&io) == 0));

    reproc_destroy(&io);
  }
}
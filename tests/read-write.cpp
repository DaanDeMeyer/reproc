#include <doctest.h>
#include <reproc/reproc.h>

#include <array>
#include <string>

TEST_CASE("read-write")
{
  reproc_type io;

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  static constexpr unsigned int BUFFER_SIZE = 1024;
  std::array<char, BUFFER_SIZE> buffer = { {} };

  SUBCASE("stdout")
  {
    std::string message = "This is stdout";
    auto message_length = static_cast<unsigned int>(message.length());

    static constexpr unsigned int ARGV_SIZE = 2;
    std::array<const char *, ARGV_SIZE> argv{ { STDOUT_PATH, nullptr } };

    error = reproc_start(&io, ARGV_SIZE - 1, argv.data(), nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, message.data(), message_length, &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_IN);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&io, REPROC_OUT, buffer.data(), BUFFER_SIZE,
                          &bytes_read);
      if (error != REPROC_SUCCESS) { break; }

      output.append(buffer.data(), bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  SUBCASE("stderr")
  {
    std::string message = "This is stderr";
    auto message_length = static_cast<unsigned int>(message.length());

    static constexpr unsigned int ARGV_SIZE = 2;
    std::array<const char *, ARGV_SIZE> argv{ { STDERR_PATH, nullptr } };

    error = reproc_start(&io, ARGV_SIZE - 1, argv.data(), nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, message.data(), message_length, &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_IN);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&io, REPROC_ERR, buffer.data(), BUFFER_SIZE,
                          &bytes_read);
      if (error != REPROC_SUCCESS) { break; }

      output.append(buffer.data(), bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  unsigned int exit_status = 0;
  error = reproc_stop(&io, REPROC_WAIT, REPROC_INFINITE, 0, 0, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));
}

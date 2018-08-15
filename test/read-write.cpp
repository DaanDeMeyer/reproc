#include <doctest.h>
#include <reproc/reproc.h>

#include <string>

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("read-write")
{
  reproc_type io;

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  static constexpr unsigned int BUFFER_SIZE = 1024;
  char buffer[BUFFER_SIZE];

  SUBCASE("stdout")
  {
    std::string message = "This is stdout";
    auto message_length = static_cast<unsigned int>(message.length());

    static constexpr unsigned int ARGV_SIZE = 2;
    const char *argv[ARGV_SIZE] = { STDOUT_PATH, nullptr };

    error = reproc_start(&io, ARGV_SIZE - 1, argv, nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, message.data(), message_length,
                         &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_IN);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&io, REPROC_OUT, buffer, BUFFER_SIZE,
                          &bytes_read);
      if (error) { break; }

      output.append(buffer, bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  SUBCASE("stderr")
  {
    std::string message = "This is stderr";
    auto message_length = static_cast<unsigned int>(message.length());

    static constexpr unsigned int ARGV_SIZE = 2;
    const char *argv[ARGV_SIZE] = { STDERR_PATH, nullptr };

    error = reproc_start(&io, ARGV_SIZE - 1, argv, nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, message.data(), message_length,
                         &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_IN);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&io, REPROC_ERR, buffer, BUFFER_SIZE,
                          &bytes_read);
      if (error) { break; }

      output.append(buffer, bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  unsigned int exit_status = 0;
  error = reproc_stop(&io, REPROC_WAIT, REPROC_INFINITE, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));
}

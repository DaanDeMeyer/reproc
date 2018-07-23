#include <doctest.h>
#include <reproc/reproc.h>

#include <array>
#include <string>

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("read-write")
{
  reproc_type io;
  reproc_init(&io);

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  static constexpr unsigned int BUFFER_SIZE = 1024;
  std::array<char, BUFFER_SIZE> buffer{};

  SUBCASE("stdout")
  {
    std::string message = "This is stdout";
    auto message_length = static_cast<unsigned int>(message.length());

    std::array<const char *, 2> argv = { { REPROC_STDOUT_HELPER, nullptr } };
    auto argc = static_cast<int>(argv.size() - 1);

    error = reproc_start(&io, argc, argv.data(), nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, message.data(), message_length,
                         &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_STDIN);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&io, REPROC_STDOUT, buffer.data(), BUFFER_SIZE,
                          &bytes_read);
      if (error) { break; }

      output.append(buffer.data(), bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  SUBCASE("stderr")
  {
    std::string message = "This is stderr";
    auto message_length = static_cast<unsigned int>(message.length());

    std::array<const char *, 2> argv = { { REPROC_STDERR_HELPER, nullptr } };
    auto argc = static_cast<int>(argv.size() - 1);

    error = reproc_start(&io, argc, argv.data(), nullptr);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&io, message.data(), message_length,
                         &bytes_written);
    REQUIRE(!error);

    reproc_close(&io, REPROC_STDIN);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&io, REPROC_STDERR, buffer.data(), BUFFER_SIZE,
                          &bytes_read);
      if (error) { break; }

      output.append(buffer.data(), bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  unsigned int exit_status = 0;
  error = reproc_wait(&io, REPROC_INFINITE, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  reproc_destroy(&io);
}

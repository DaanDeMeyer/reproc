#include <doctest.h>
#include <reproc/reproc.h>

#include <array>
#include <string>

static constexpr unsigned int BUFFER_SIZE = 1024;

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("read-write")
{
  reproc_type reproc;

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_init(&reproc);
  REQUIRE(!error);

  std::array<char, BUFFER_SIZE> buffer{};

  SUBCASE("stdout")
  {
    std::string message = "This is stdout";
    auto message_length = static_cast<unsigned int>(message.length());

    std::array<const char *, 2> argv = { { STDOUT_PATH, nullptr } };
    auto argc = static_cast<int>(argv.size() - 1);

    error = reproc_start(&reproc, argc, argv.data(), NOOP_DIR);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&reproc, message.data(), message_length,
                         &bytes_written);
    REQUIRE(!error);

    error = reproc_close_stdin(&reproc);
    REQUIRE(!error);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&reproc, buffer.data(), BUFFER_SIZE, &bytes_read);
      if (error) { break; }

      output.append(buffer.data(), bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  SUBCASE("stderr")
  {
    std::string message = "This is stderr";
    auto message_length = static_cast<unsigned int>(message.length());

    std::array<const char *, 2> argv = { { STDERR_PATH, nullptr } };
    auto argc = static_cast<int>(argv.size() - 1);

    error = reproc_start(&reproc, argc, argv.data(), NOOP_DIR);
    REQUIRE(!error);

    unsigned int bytes_written = 0;
    error = reproc_write(&reproc, message.data(), message_length,
                         &bytes_written);
    REQUIRE(!error);

    error = reproc_close_stdin(&reproc);
    REQUIRE(!error);

    std::string output{};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read_stderr(&reproc, buffer.data(), BUFFER_SIZE,
                                 &bytes_read);
      if (error) { break; }

      output.append(buffer.data(), bytes_read);
    }

    REQUIRE_EQ(output, message);
  }

  error = reproc_wait(&reproc, REPROC_INFINITE);
  REQUIRE(!error);

  int exit_status = 0;
  error = reproc_exit_status(&reproc, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = reproc_destroy(&reproc);
  REQUIRE(!error);
}

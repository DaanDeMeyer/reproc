#include <doctest.h>
#include <process.h>

#include <array>
#include <cstdlib>
#include <sstream>

// Alternates between writing to stdin and reading from stdout and writing to
// stdin and reading from stderr.
// See res/echo for the child process code
TEST_CASE("read-write")
{
  auto process = static_cast<process_type *>(malloc(process_size()));
  REQUIRE(process);

  PROCESS_LIB_ERROR error;
  CAPTURE(error);

  error = process_init(process);
  REQUIRE(!error);

  std::array<char, 1000> buffer{};
  unsigned int actual = 0;

  SUBCASE("stdout")
  {
    std::string message = "This is stdout";

    int argc = 2;
    const char *argv[3] = { ECHO_PATH, "stdout", nullptr };

    error = process_start(process, argc, argv, NOOP_DIR);
    REQUIRE(!error);

    error = process_write(process, message.data(), message.length(), &actual);
    REQUIRE(!error);

    error = process_close_stdin(process);
    REQUIRE(!error);

    std::stringstream ss;

    while (true) {
      error = process_read(process, buffer.data(), buffer.size() - 1, &actual);
      if (error) { break; }

      buffer[actual] = '\0';
      ss << buffer.data();
    }

    REQUIRE_EQ(ss.str(), message);
  }

  SUBCASE("stderr")
  {
    std::string message = "This is stderr";

    int argc = 2;
    const char *argv[3] = { ECHO_PATH, "stderr", nullptr };

    error = process_start(process, argc, argv, NOOP_DIR);
    REQUIRE(!error);

    error = process_write(process, message.data(), message.length(), &actual);
    REQUIRE(!error);

    error = process_close_stdin(process);
    REQUIRE(!error);

    std::stringstream ss;

    while (true) {
      error = process_read_stderr(process, buffer.data(), buffer.size() - 1,
                                  &actual);
      if (error) { break; }

      buffer[actual] = '\0';
      ss << buffer.data();
    }

    REQUIRE_EQ(ss.str(), message);
  }

  error = process_wait(process, PROCESS_LIB_INFINITE);
  REQUIRE(!error);

  int exit_status;
  error = process_exit_status(process, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = process_destroy(process);
  REQUIRE(!error);

  free(process);
}
